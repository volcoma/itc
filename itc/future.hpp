#pragma once

#include "thread.h"
#include "utility.hpp"
#include <chrono>
#include <functional>
#include <future>
#include <thread>
namespace itc
{

enum class future_status : unsigned
{
	not_ready,
	ready,
	error
};

template <typename T>
class future;

namespace detail
{
template <typename T>
struct internal_state
{
	std::shared_ptr<T> value;
	std::atomic<std::thread::id> thread_id = {};
	std::atomic<future_status> status = {future_status::not_ready};
};
template <typename T>
class promise_base;

template <typename T>
class future_base
{
	friend class promise_base<T>;

public:
	future_base() = default;
	future_base(future_base&& rhs) noexcept = default;
	future_base& operator=(future_base&& rhs) noexcept = default;

	future_base(const future_base&) = delete;
	future_base& operator=(const future_base&) = delete;

	//-----------------------------------------------------------------------------
	/// Checks if the future has an associated shared state.
	/// This is the case only for futures that were not default-constructed or
	/// moved from (i.e. returned by promise::get_future()
	//-----------------------------------------------------------------------------
	bool valid() const
	{
		return state_ != nullptr;
	}

	//-----------------------------------------------------------------------------
	/// Checks whether the result is ready.
	//-----------------------------------------------------------------------------
	bool is_ready() const
	{
		return state_->status == future_status::ready;
	}

	//-----------------------------------------------------------------------------
	/// Checks whether an error occurred.
	//-----------------------------------------------------------------------------
	bool has_error() const
	{
		return state_->status == future_status::error;
	}

	//-----------------------------------------------------------------------------
	/// Waits for the result to become available
	//-----------------------------------------------------------------------------
	void wait() const
	{
		auto current_id = std::this_thread::get_id();
		std::thread::id invalid;
		auto id = state_->thread_id.exchange(current_id);
		if(id == invalid)
		{
			while(state_->status == future_status::not_ready)
			{
				if(this_thread::notified_for_exit())
				{
					break;
				}
				this_thread::wait_for_event();
			}
		}
	}

	//-----------------------------------------------------------------------------
	/// Waits for the result to become available.
	/// Blocks until specified timeout_duration has elapsed or
	/// the result becomes available, whichever comes first.
	/// Returns value identifies the state of the result.
	/// This function may block for longer than timeout_duration
	/// due to scheduling or resource contention delays.
	//-----------------------------------------------------------------------------
	template <class Rep, class Per>
	future_status wait_for(const std::chrono::duration<Rep, Per>& timeout_duration) const
	{
		auto now = clock::now();
		auto end_time = now + timeout_duration;

		auto current_id = std::this_thread::get_id();
		std::thread::id invalid;
		auto id = state_->thread_id.exchange(current_id);
		if(id == invalid)
		{
			while(state_->status == future_status::not_ready && now < end_time)
			{
				if(this_thread::notified_for_exit())
				{
					break;
				}
				auto time_left = end_time - now;
				this_thread::wait_for_event(time_left);

				now = clock::now();
			}
		}

		return state_->status;
	}

	template <class Clock, class Duration>
	future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const
	{
		return wait_for(abs_time.time_since_epoch() - Clock::now().time_since_epoch());
	}

protected:
	std::shared_ptr<internal_state<T>> state_;
};

template <typename T>
class promise_base
{
public:
	promise_base() = default;
	promise_base(promise_base&& rhs) noexcept = default;
	promise_base& operator=(promise_base&& rhs) noexcept = default;

	promise_base(const promise_base&) = delete;
	promise_base& operator=(const promise_base&) = delete;

	~promise_base()
	{
		if(state_ && state_->status == future_status::not_ready)
		{
			set_status(future_status::error);
		}
	}

	//-----------------------------------------------------------------------------
	/// Returns a future associated with the promised result
	//-----------------------------------------------------------------------------
	future<T> get_future()
	{
		future<T> f;
		f.state_ = this->state_;
		return f;
	}

protected:
	void set_status(future_status status)
	{
		if(!state_)
		{
			return;
		}
		state_->status = status;

		std::thread::id invalid;
		auto id = state_->thread_id.exchange(invalid);
		// if waiter thread is set then notify it
		if(id != invalid)
		{
			notify(id);
		}
	}

	/// The shared state
	std::shared_ptr<internal_state<T>> state_ = std::make_shared<internal_state<T>>();
};
}

template <typename T>
class future : public detail::future_base<T>
{
public:
	T& get() const
	{
		this->wait();

		auto value = std::move(this->state_->value);
		return *value.get();
	}
};

template <>
class future<void> : public detail::future_base<void>
{
	void get() const
	{
		wait();
	}
};

template <typename T>
class promise : public detail::promise_base<T>
{
public:
	//-----------------------------------------------------------------------------
	/// Sets the result to specific value
	//-----------------------------------------------------------------------------
	void set_value(T&& value)
	{
		if(!this->state_)
		{
			return;
		}
		this->state_->value = std::make_shared<T>(std::move(value));
		this->set_status(future_status::ready);
	}

	//-----------------------------------------------------------------------------
	/// Sets the result to specific value
	//-----------------------------------------------------------------------------
	void set_value(const T& value)
	{
		if(!this->state_)
		{
			return;
		}
		this->state_->value = std::make_shared<T>(value);
		this->set_status(future_status::ready);
	}
};
template <>
class promise<void> : public detail::promise_base<void>
{
public:
	//-----------------------------------------------------------------------------
	/// Sets the result to specific value
	//-----------------------------------------------------------------------------
	void set_value()
	{
		set_status(future_status::ready);
	}
};
}
