#pragma once
#include "detail/semaphore.h"

namespace itc
{
class condition_variable
{

public:
	condition_variable() = default;

	condition_variable(condition_variable&& rhs) = delete;
	condition_variable& operator=(condition_variable&& rhs) = delete;

	condition_variable(const condition_variable&) = delete;
	condition_variable& operator=(const condition_variable&) = delete;

	//-----------------------------------------------------------------------------
	/// If any threads are waiting on *this,
	/// calling notify_one unblocks one of the waiting threads.
	//-----------------------------------------------------------------------------
	void notify_one() noexcept
	{
		sync_.notify_one();
	}

	//-----------------------------------------------------------------------------
	/// Unblocks all threads currently waiting for *this.
	//-----------------------------------------------------------------------------
	void notify_all() noexcept
	{
		sync_.notify_all();
	}

	//-----------------------------------------------------------------------------
	/// Waits for the result to become available
	//-----------------------------------------------------------------------------
	void wait(std::unique_lock<std::mutex>& lock) const
	{
		const auto before_wait = [&lock]() { lock.unlock(); };
		const auto after_wait = [&lock]() { lock.lock(); };

		sync_.wait(before_wait, after_wait);
	}

	//-----------------------------------------------------------------------------
	/// Blocks until specified timeout_duration has elapsed or
	/// notified, whichever comes first.
	/// Returns value identifies the state of the result.
	/// This function may block for longer than timeout_duration
	/// due to scheduling or resource contention delays.
	//-----------------------------------------------------------------------------
	template <class Rep, class Per>
	std::cv_status wait_for(std::unique_lock<std::mutex>& lock,
							const std::chrono::duration<Rep, Per>& timeout_duration) const
	{
		const auto before_wait = [&lock]() { lock.unlock(); };
		const auto after_wait = [&lock]() { lock.lock(); };

		return sync_.wait_for(timeout_duration, before_wait, after_wait);
	}

	//-----------------------------------------------------------------------------
	/// Blocks until specified abs_time is reached or
	/// notified, whichever comes first.
	/// Returns value identifies the state of the result.
	/// This function may block for longer than timeout_duration
	/// due to scheduling or resource contention delays.
	//-----------------------------------------------------------------------------
	template <class Clock, class Duration>
	std::cv_status wait_until(std::unique_lock<std::mutex>& lock,
							  const std::chrono::time_point<Clock, Duration>& abs_time) const
	{
		return wait_for(lock, abs_time.time_since_epoch() - Clock::now().time_since_epoch());
	}

private:
	detail::semaphore sync_;
};
} // namespace itc
