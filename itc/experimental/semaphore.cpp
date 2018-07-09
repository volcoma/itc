#include "semaphore.h"
#include "../thread.h"
#include <algorithm>
namespace itc
{
namespace experimental
{
void semaphore::notify_one() noexcept
{
    auto waiter = [&]()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if(!waiters_.empty())
        {
            auto waiter = std::move(waiters_.back());
            waiters_.pop_back();

            return waiter;
        }

        return thread_info();
    }();

	if(waiter.is_valid())
	{
        waiter.flag->store(true);

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

	for(const auto& waiter : waiters)
	{
        if(waiter.is_valid())
        {
            waiter.flag->store(true);

            notify(waiter.id);
        }
	}
}

void semaphore::wait(const callback& before_wait, const callback& after_wait) const
{
    auto id = std::this_thread::get_id();

    /// flag is shared_ptr of atomic bool
    /// note: we may be the last reference to the
    /// flag as the notify_functions will set it
    /// and erase it from the container
	auto notified_flag = add_to_waiters(id);

	while(!(notified_flag->load()))
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
}

std::cv_status semaphore::wait_for_impl(const std::chrono::nanoseconds& timeout_duration, const callback& before_wait,
							  const callback& after_wait) const
{
    auto status = std::cv_status::no_timeout;
	auto now = clock::now();
	auto end_time = now + timeout_duration;

	auto id = std::this_thread::get_id();

    /// flag is shared_ptr of atomic bool
    /// note: we may be the last reference to the
    /// flag as the notify_functions will set it
    /// and erase it from the container
	auto notified_flag = add_to_waiters(id);

	while(!(notified_flag->load()))
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

    return status;
}

semaphore::notification_flag semaphore::add_to_waiters(thread::id id) const
{
    std::unique_lock<std::mutex> waiting_lock(mutex_);

    auto it = std::find_if(std::begin(waiters_), std::end(waiters_), [&id](const thread_info& element)
    {
        return element.id == id;
    });
    if(it != std::end(waiters_))
    {
        return it->flag;
    }

    thread_info info;
    info.id = id;
    info.flag = std::make_shared<std::atomic<bool>>(false);
    waiters_.emplace_back(std::move(info));
    return waiters_.back().flag;
}

}
}
