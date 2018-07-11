#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <thread>

namespace itc
{
namespace experimental
{
class semaphore
{
public:
	using callback = std::function<void()>;

	semaphore() = default;

	// mutex prevents us from being moveable
	semaphore(semaphore&& rhs) = delete;
	semaphore& operator=(semaphore&& rhs) = delete;

	semaphore(const semaphore&) = delete;
	semaphore& operator=(const semaphore&) = delete;

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
	template <class Rep, class Per>
	std::cv_status wait_for(const std::chrono::duration<Rep, Per>& timeout_duration,
							const callback& before_wait = nullptr, const callback& after_wait = nullptr) const
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
	template <class Clock, class Duration>
	std::cv_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time,
							  const callback& before_wait = nullptr,
							  const callback& after_wait = nullptr) const
	{
		return wait_for(abs_time.time_since_epoch() - Clock::now().time_since_epoch(), before_wait,
						after_wait);
	}

protected:
	using notification_flag = std::shared_ptr<std::atomic<bool>>;

	struct waiter_info
	{
		notification_flag flag;
		std::thread::id id;
	};

	std::cv_status wait_for_impl(const std::chrono::nanoseconds& timeout_duration,
								 const callback& before_wait, const callback& after_wait) const;

	notification_flag add_waiter(std::thread::id id) const;

	void remove_waiter(std::thread::id id) const;

	mutable std::mutex mutex_;
	/// This container should be relatively small so
	/// vector will do it.
	mutable std::list<waiter_info> waiters_;
};
}
}
