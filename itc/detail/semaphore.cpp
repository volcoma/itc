#include "semaphore.h"
#include <algorithm>

namespace itc
{
namespace detail
{

interrupt_token::interrupt_token(bool b) : flag_{std::make_shared<std::atomic<bool>>(b)}
{
}

auto interrupt_token::interrupt() noexcept -> bool
{
    assert(flag_ != nullptr);
    return flag_->exchange(true);
}

auto interrupt_token::is_interrupted() const noexcept -> bool
{
    assert(flag_ != nullptr);
    return flag_->load();
}

void semaphore::notify_one() noexcept
{
    auto waiter = [&]()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if(!waiters_.empty())
        {
            // The standard doesn't specify any order/priority
            // here. We are fair and wake the oldest one
            auto waiter = std::move(waiters_.front());
            waiters_.erase(waiters_.begin());

            return waiter;
        }

        return waiter_info();
    }();

    if(waiter.id != invalid_id())
    {
        waiter.token.interrupt();

        notify(waiter.id);
    }
}

void semaphore::notify_all() noexcept
{
    auto waiters = [&]()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        return std::move(waiters_);
    }();

    for(auto& waiter : waiters)
    {
        if(waiter.id != invalid_id())
        {
            waiter.token.interrupt();

            notify(waiter.id);
        }
    }
}

void semaphore::wait(const callback& before_wait, const callback& after_wait) const
{
    auto id = this_thread::get_id();

    /// flag is shared_ptr of atomic bool
    /// note: we may be the last reference to the
    /// flag as the notify_functions will set it
    /// and erase it from the container
    auto token = add_waiter(id);

    while(!token.is_interrupted())
    {
        if(this_thread::notified_for_exit())
        {
            break;
        }

        if(before_wait)
        {
            before_wait();
        }

        this_thread::wait();

        if(after_wait)
        {
            after_wait();
        }
    }

    remove_waiter(id);
}

auto semaphore::wait_for_impl(const std::chrono::nanoseconds& timeout_duration,
                              const callback& before_wait,
                              const callback& after_wait) const -> std::cv_status
{
    auto status = std::cv_status::no_timeout;
    auto now = clock::now();
    auto end_time = now + timeout_duration;

    auto id = this_thread::get_id();

    /// flag is shared_ptr of atomic bool
    /// note: we may be the last reference to the
    /// flag as the notify_functions will set it
    /// and erase it from the container
    auto token = add_waiter(id);

    while(!token.is_interrupted())
    {
        if(now >= end_time)
        {
            status = std::cv_status::timeout;
            break;
        }

        if(this_thread::notified_for_exit())
        {
            break;
        }
        auto time_left = end_time - now;

        if(before_wait)
        {
            before_wait();
        }

        status = this_thread::wait_for(time_left);

        if(after_wait)
        {
            after_wait();
        }

        now = clock::now();
    }

    // remove waiter in case of timeout
    remove_waiter(id);

    return status;
}

auto semaphore::add_waiter(thread::id id) const -> interrupt_token
{
    std::unique_lock<std::mutex> waiting_lock(mutex_);

    auto it = std::find_if(std::begin(waiters_),
                           std::end(waiters_),
                           [&id](const waiter_info& waiter)
                           {
                               return waiter.id == id;
                           });
    if(it != std::end(waiters_))
    {
        return it->token;
    }

    waiter_info info;
    info.id = id;
    info.token = interrupt_token{false};
    waiters_.emplace_back(std::move(info));
    return waiters_.back().token;
}

void semaphore::remove_waiter(thread::id id) const
{
    std::unique_lock<std::mutex> waiting_lock(mutex_);

    waiters_.erase(std::remove_if(std::begin(waiters_),
                                  std::end(waiters_),
                                  [&id](const waiter_info& waiter)
                                  {
                                      return waiter.id == id;
                                  }),
                   std::end(waiters_));
}

} // namespace detail
} // namespace itc
