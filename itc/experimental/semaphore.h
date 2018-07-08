#pragma once
#include <chrono>
#include <functional>
#include <map>
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
	semaphore(semaphore&& rhs) = default;
	semaphore& operator=(semaphore&& rhs) = default;

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
	void wait(const callback& before_wait, const callback& after_wait) const;
	//-----------------------------------------------------------------------------
	/// Blocks until specified timeout_duration has elapsed or
	/// notified, whichever comes first.
	/// Returns value identifies the state of the result.
	/// This function may block for longer than timeout_duration
	/// due to scheduling or resource contention delays.
	//-----------------------------------------------------------------------------
	template <class Rep, class Per>
	void wait_for(const std::chrono::duration<Rep, Per>& timeout_duration, const callback& before_wait,
				  const callback& after_wait) const
	{
		wait_for_impl(timeout_duration, before_wait, after_wait);
	}

	//-----------------------------------------------------------------------------
	/// Blocks until specified abs_time is reached or
	/// notified, whichever comes first.
	/// Returns value identifies the state of the result.
	/// This function may block for longer than timeout_duration
	/// due to scheduling or resource contention delays.
	//-----------------------------------------------------------------------------
	template <class Clock, class Duration>
	void wait_until(const std::chrono::time_point<Clock, Duration>& abs_time, const callback& before_wait,
					const callback& after_wait) const
	{
		wait_for(abs_time.time_since_epoch() - Clock::now().time_since_epoch(), before_wait, after_wait);
	}

protected:
	void wait_for_impl(const std::chrono::nanoseconds& timeout_duration, const callback& before_wait,
					   const callback& after_wait) const;

	bool& add_to_waiters(std::thread::id id) const;

	size_t notify_one_impl(std::unique_lock<std::mutex>& lock) noexcept;

	mutable std::mutex mutex_;
	/// container of waiting threads
	mutable std::map<std::thread::id, bool> waiting_threads_;
};
}
}
