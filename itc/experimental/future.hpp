#pragma once

#include "../thread.h"
#include "../utility.hpp"
#include "semaphore.h"
#include <chrono>
#include <functional>
#include <future>
#include <thread>
namespace itc
{
namespace experimental
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
    semaphore sync;
	std::shared_ptr<T> value;
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

	future_base(const future_base&) = default;
	future_base& operator=(const future_base&) = default;

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
		while(state_->status == future_status::not_ready)
		{
			if(this_thread::notified_for_exit())
			{
				break;
			}

			state_->sync.wait();
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
	template <typename Rep, typename Per>
	future_status wait_for(const std::chrono::duration<Rep, Per>& timeout_duration) const
	{
		while(state_->status == future_status::not_ready)
		{
			if(this_thread::notified_for_exit())
			{
				break;
			}

			if(state_->sync.wait_for(timeout_duration) == std::cv_status::timeout)
			{
				break;
			}
		}

		return state_->status;
	}

	template <typename Clock, typename Duration>
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

		state_->sync.notify_all();
	}

	/// The shared state
	std::shared_ptr<internal_state<T>> state_ = std::make_shared<internal_state<T>>();
};
}

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

template <typename T>
class future;

template <typename T>
class shared_future : public detail::future_base<T>
{
	friend class future<T>;

public:
	const T& get() const
	{
		this->wait();

		auto value = this->state_->value;
		return *value;
	}
};

template <>
class shared_future<void> : public detail::future_base<void>
{
	friend class future<void>;

public:
	void get() const
	{
		wait();
	}
};

template <typename T>
class future : public detail::future_base<T>
{
public:
	future() = default;
	future(future&& rhs) noexcept = default;
	future& operator=(future&& rhs) noexcept = default;

	future(const future&) = delete;
	future& operator=(const future&) = delete;

	T get() const
	{
		this->wait();

		auto value = std::move(this->state_->value);
		return *value;
	}

	shared_future<T> share()
	{
		shared_future<T> sf;
		sf.state_ = std::move(this->state_);
		return sf;
	}
};

template <>
class future<void> : public detail::future_base<void>
{
public:
	future() = default;
	future(future&& rhs) noexcept = default;
	future& operator=(future&& rhs) noexcept = default;

	future(const future&) = delete;
	future& operator=(const future&) = delete;

	void get() const
	{
		wait();

		state_->value.reset();
	}

	shared_future<void> share()
	{
		shared_future<void> sf;
		sf.state_ = std::move(this->state_);
		return sf;
	}
};
}
}
