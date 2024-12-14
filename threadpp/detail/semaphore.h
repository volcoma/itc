#pragma once
#include "../thread.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>

namespace tpp
{
namespace detail
{

// based on p0660r2
class interrupt_token
{
public:
    explicit interrupt_token() = default;
    explicit interrupt_token(bool b);

    auto interrupt() noexcept -> bool;
    auto is_interrupted() const noexcept -> bool;

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

class semaphore
{
public:
    using callback = std::function<void()>;

    semaphore() noexcept = default;

    // mutex prevents us from being moveable
    semaphore(semaphore&& rhs) = delete;
    auto operator=(semaphore&& rhs) -> semaphore& = delete;

    semaphore(const semaphore&) = delete;
    auto operator=(const semaphore&) -> semaphore& = delete;

    //-----------------------------------------------------------------------------
    /// If any threads are waiting on *this,
    /// calling notify_one unblocks one of the waiting threads.
    //-----------------------------------------------------------------------------
    void notify_one() noexcept;

    //-----------------------------------------------------------------------------
    /// Unblocks all threads currently waiting for *this.
    //-----------------------------------------------------------------------------
    void notify_all() noexcept;

    //-----------------------------------------------------------------------------
    /// Waits for the result to become available
    //-----------------------------------------------------------------------------
    void wait(const callback& before_wait = nullptr, const callback& after_wait = nullptr) const;

    //-----------------------------------------------------------------------------
    /// Blocks until specified timeout_duration has elapsed or
    /// notified, whichever comes first.
    /// Returns value identifies the state of the result.
    /// This function may block for longer than timeout_duration
    /// due to scheduling or resource contention delays.
    //-----------------------------------------------------------------------------
    template<class Rep, class Per>
    auto wait_for(const std::chrono::duration<Rep, Per>& timeout_duration,
                  const callback& before_wait = nullptr,
                  const callback& after_wait = nullptr) const -> std::cv_status
    {
        return wait_for_impl(timeout_duration, before_wait, after_wait);
    }

    //-----------------------------------------------------------------------------
    /// Blocks until specified abs_time is reached or
    /// notified, whichever comes first.
    /// Returns value identifies the state of the result.
    /// This function may block for longer than timeout_duration
    /// due to scheduling or resource contention delays.
    //-----------------------------------------------------------------------------
    template<class Clock, class Duration>
    auto wait_until(const std::chrono::time_point<Clock, Duration>& abs_time,
                    const callback& before_wait = nullptr,
                    const callback& after_wait = nullptr) const -> std::cv_status
    {
        return wait_for(abs_time.time_since_epoch() - Clock::now().time_since_epoch(), before_wait, after_wait);
    }

protected:
    struct waiter_info
    {
        interrupt_token token;
        thread::id id;
    };

    auto wait_for_impl(const std::chrono::nanoseconds& timeout_duration,
                       const callback& before_wait,
                       const callback& after_wait) const -> std::cv_status;

    auto add_waiter(thread::id id) const -> interrupt_token;

    void remove_waiter(thread::id id) const;

    mutable std::mutex mutex_;
    mutable std::vector<waiter_info> waiters_;
};
} // namespace detail
} // namespace tpp
