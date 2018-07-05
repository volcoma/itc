#pragma once

#include "itc.h"
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <thread>
namespace itc
{
template <class T>
class move_on_copy_t
{
public:
	move_on_copy_t& operator=(const move_on_copy_t<T>& other) = delete;
	move_on_copy_t& operator=(const move_on_copy_t<T>&& other) = delete;

	move_on_copy_t(T&& value) noexcept
		: _value(std::move(value))
	{
	}
	move_on_copy_t(const move_on_copy_t& other) noexcept
		: _value(std::move(other._value))
	{
	}

	const T& get() const
	{
		return _value;
	}
	T& get()
	{
		return _value;
	}

private:
	mutable T _value;
};

template <typename T>
move_on_copy_t<T> make_move_on_copy(T&& value)
{
	return move_on_copy_t<T>(std::forward<T>(value));
}
template <typename T>
move_on_copy_t<T> monc(T& value)
{
	return make_move_on_copy(std::move(value));
}
template <typename T>
move_on_copy_t<T> capture(T& value)
{
	return monc(value);
}

enum class future_status : unsigned
{
	not_ready,
	ready,
	error
};

template <typename T>
class promise;

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
class future_base
{
	friend class promise<T>;

public:
	future_base() = default;
	future_base(future_base&& rhs) = default;
	future_base& operator=(future_base&& rhs) = default;

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

		while(state_->status == future_status::not_ready)
		{
			this_thread::wait_for_event();
		}
	}

	//-----------------------------------------------------------------------------
	/// waits for the result, returns if it is not available for
	/// the specified timeout duration
	//-----------------------------------------------------------------------------
	template <class Rep, class Per>
	future_status wait_for(const std::chrono::duration<Rep, Per>& rtime) const
	{
		auto now = clock::now();
		auto end_time = now + rtime;

		while(state_->status == future_status::not_ready && now < end_time)
		{
			if(this_thread::notified_for_exit())
			{
				break;
			}
			auto time_left = end_time - now;

			wait_for_event(time_left);

			now = clock::now();
		}

		return state_->status;
	}

protected:
	std::shared_ptr<internal_state<T>> state_;
};

template <typename T>
class promise_base
{
public:
	promise_base() = default;
	promise_base(promise_base&& rhs) = default;
	promise_base& operator=(promise_base&& rhs) = default;

	promise_base(const promise_base&) = delete;
	promise_base& operator=(const promise_base&) = delete;

	~promise_base()
	{
		if(state_ && state_->status == future_status::not_ready)
		{
			set_status(future_status::error);
		}
	}

protected:
	void set_status(future_status status)
	{
		if(!state_)
		{
			return;
		}
		state_->status = status;

		notify(state_->thread_id);
	}

	/// The shared state
	std::shared_ptr<internal_state<T>> state_ = std::make_shared<internal_state<T>>();
};
}

template <typename T>
class future : public detail::future_base<T>
{
public:
	decltype(auto) get() const
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
	/// Returns a future associated with the promised result
	//-----------------------------------------------------------------------------
	future<T> get_future()
	{
		future<T> f;
		f.state_ = this->state_;
		return f;
	}

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
	/// Returns a future associated with the promised result
	//-----------------------------------------------------------------------------
	future<void> get_future()
	{
		future<void> f;
		f.state_ = this->state_;
		return f;
	}

	//-----------------------------------------------------------------------------
	/// Sets the result to specific value
	//-----------------------------------------------------------------------------
	void set_value()
	{
		set_status(future_status::ready);
	}
};
}
