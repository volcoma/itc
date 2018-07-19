#pragma once
#include "../condition_variable.hpp"
#include <future>

namespace itc
{

namespace detail
{

enum class value_status : unsigned
{
	not_set,
	ready,
	error
};

template <typename T>
struct future_shared_state
{
	condition_variable cv;
	mutable std::mutex guard;
	task callback;

	std::shared_ptr<T> value;
	std::exception_ptr exception;

	std::atomic<value_status> status = {value_status::not_set};

	std::atomic_flag retrieved = ATOMIC_FLAG_INIT;

	bool ready() const
	{
		return status != value_status::not_set;
	}

	bool has_error() const
	{
		return status == value_status::error;
	}

	template <typename V>
	void set_value(V&& val)
	{
		std::unique_lock<std::mutex> lock(guard);

		if(ready())
		{
			throw std::future_error(std::future_errc::promise_already_satisfied);
		}

		value = std::make_shared<V>(std::forward<V>(val));
		set_ready(lock, value_status::ready);
	}

	void set_value()
	{
		std::unique_lock<std::mutex> lock(guard);

		if(ready())
		{
			throw std::future_error(std::future_errc::promise_already_satisfied);
		}

		set_ready(lock, value_status::ready);
	}

	void set_exception(std::exception_ptr ex)
	{
		std::unique_lock<std::mutex> lock(guard);

		if(ready())
		{
			throw std::future_error(std::future_errc::promise_already_satisfied);
		}
		exception = std::move(ex);
		set_ready(lock, value_status::error);
	}

	void set_ready(std::unique_lock<std::mutex>& lock, value_status s)
	{
		task continuation;
		{
			status = s;
			callback.swap(continuation);
			cv.notify_all();
		}

		lock.unlock();
		if(continuation)
		{
			continuation();
		}
	}

	void set_continuation(task continuation)
	{
		bool just_run_it = false;
		{
			std::lock_guard<std::mutex> lock(guard);
			just_run_it = ready();
			if(!just_run_it)
			{
				callback.swap(continuation);
			}
		}
		if(just_run_it)
		{
			continuation();
		}
	}

	void wait()
	{
		std::unique_lock<std::mutex> lock(guard);
		while(!ready())
		{
			if(this_thread::notified_for_exit())
			{
				break;
			}
			cv.wait(lock);
		}
	}

	template <typename Rep, typename Per>
	std::future_status wait_for(const std::chrono::duration<Rep, Per>& timeout_duration) const
	{
		std::unique_lock<std::mutex> lock(guard);

		while(!ready())
		{
			if(this_thread::notified_for_exit())
			{
				return std::future_status::deferred;
			}

			if(cv.wait_for(lock, timeout_duration) == std::cv_status::timeout)
			{
				return std::future_status::timeout;
			}
		}

		return std::future_status::ready;
	}

	decltype(auto) get_value_assuming_ready()
	{
		if(value)
		{
			return *value;
		}
		else
		{
			if(exception)
			{
				std::rethrow_exception(exception);
			}
			else
			{
				throw std::future_error(std::future_errc::broken_promise);
			}
		}
	}

	void rethrow_any_exception() const
	{
		if(exception)
		{
			std::rethrow_exception(exception);
		}
	}
};

template <typename T>
inline void expect_state(const std::shared_ptr<future_shared_state<T>>& state)
{
	if(!state)
	{
		throw std::future_error(std::future_errc::no_state);
	}
}

template <typename T>
inline void state_invalidate(std::shared_ptr<future_shared_state<T>>& state)
{
	state.reset();
}
}
} // namespace itc
