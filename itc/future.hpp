#pragma once
#include "detail/semaphore.h"

#include "thread.h"
#include <future>

namespace itc
{

/// The implementations here are based on and inspired by libstd++
template <typename T>
class future;
template <typename T>
class shared_future;
template <typename T>
class promise;

namespace detail
{

enum class value_status : unsigned
{
	not_set,
	ready,
	error
};

template <typename T>
struct internal_state
{
	semaphore sync;

	std::shared_ptr<T> value;
	std::exception_ptr exception;
	std::atomic<value_status> status = {value_status::not_set};

	// These are here so that constructors of both
	// future and promise can remain noexcept
	std::atomic_flag once = ATOMIC_FLAG_INIT;
	std::atomic_flag retrieved = ATOMIC_FLAG_INIT;
};

template <typename T>
inline void state_check(const std::shared_ptr<internal_state<T>>& state)
{
	if(!state)
	{
		throw std::future_error(std::future_errc::no_state);
	}
}

template <typename T>
inline void state_invalidate(std::shared_ptr<internal_state<T>>& state)
{
	state.reset();
}

template <typename T>
inline bool nothing_is_set(const std::shared_ptr<internal_state<T>>& state)
{
	return state->status == value_status::not_set;
}

template <typename T>
class basic_promise;

template <typename T>
class basic_future
{
	friend class basic_promise<T>;

public:
	basic_future() = default;
	basic_future(basic_future&& rhs) noexcept = default;
	basic_future& operator=(basic_future&& rhs) noexcept = default;

	basic_future(const basic_future&) = default;
	basic_future& operator=(const basic_future&) = default;

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
		state_check(state_);

		return state_->status == value_status::ready;
	}

	//-----------------------------------------------------------------------------
	/// Checks whether an error occurred.
	//-----------------------------------------------------------------------------
	bool has_error() const
	{
		state_check(state_);

		return state_->status == value_status::error;
	}

	//-----------------------------------------------------------------------------
	/// Waits for the result to become available
	//-----------------------------------------------------------------------------
	void wait() const
	{
		state_check(state_);

		while(nothing_is_set(state_))
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
	std::future_status wait_for(const std::chrono::duration<Rep, Per>& timeout_duration) const
	{
		state_check(state_);

		while(nothing_is_set(state_))
		{
			if(this_thread::notified_for_exit())
			{
				return std::future_status::deferred;
			}

			if(state_->sync.wait_for(timeout_duration) == std::cv_status::timeout)
			{
				return std::future_status::timeout;
			}
		}

		return std::future_status::ready;
	}

	template <typename Clock, typename Duration>
	std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const
	{
		return wait_for(abs_time.time_since_epoch() - Clock::now().time_since_epoch());
	}

protected:
	void rethrow_any_exception() const
	{
		state_check(state_);

		auto exception = state_->exception;

		if(exception)
		{
			std::rethrow_exception(exception);
		}
	}

	/// The shared state
	std::shared_ptr<internal_state<T>> state_;
};

template <typename T>
class basic_promise
{
public:
	basic_promise() = default;
	basic_promise(basic_promise&& rhs) noexcept = default;
	basic_promise& operator=(basic_promise&& rhs) noexcept = default;

	basic_promise(const basic_promise&) = delete;
	basic_promise& operator=(const basic_promise&) = delete;

	~basic_promise()
	{
		if(state_ && nothing_is_set(state_))
		{
			auto exception = std::make_exception_ptr(std::future_error(std::future_errc::broken_promise));
			set_exception(exception);
		}
	}

	void set_exception(std::exception_ptr p)
	{
		auto set_impl = [this, &p]() { state_->exception = p; };
		set_value_and_status(set_impl, value_status::error);
	}

	//-----------------------------------------------------------------------------
	/// Returns a future associated with the promised result
	//-----------------------------------------------------------------------------
	future<T> get_future()
	{
		state_check(state_);
		set_retrieved_flag();

		future<T> f;
		f.state_ = state_;
		return f;
	}

protected:
	void set_status(value_status status)
	{
		state_->status = status;

		state_->sync.notify_all();
	}

	void set_retrieved_flag()
	{
		if(state_->retrieved.test_and_set())
		{
			throw std::future_error(std::future_errc::future_already_retrieved);
		}
	}

	void set_value_and_status(const std::function<void()>& f, value_status status)
	{
		state_check(state_);

		if(!state_->once.test_and_set())
		{
			f();
			set_status(status);
		}
		else
		{
			throw std::future_error(std::future_errc::promise_already_satisfied);
		}
	}

	/// The shared state
	std::shared_ptr<internal_state<T>> state_ = std::make_shared<internal_state<T>>();
};
} // namespace detail

template <typename T>
class promise : public detail::basic_promise<T>
{
public:
	//-----------------------------------------------------------------------------
	/// Sets the result to specific value
	//-----------------------------------------------------------------------------
	void set_value(T&& value)
	{
		const auto set_impl = [this, &value]() {
			this->state_->value = std::make_shared<T>(std::move(value));
		};

		this->set_value_and_status(set_impl, detail::value_status::ready);
	}

	//-----------------------------------------------------------------------------
	/// Sets the result to specific value
	//-----------------------------------------------------------------------------
	void set_value(const T& value)
	{
		const auto set_impl = [this, &value]() { this->state_->value = std::make_shared<T>(value); };

		this->set_value_and_status(set_impl, detail::value_status::ready);
	}
};
template <>
class promise<void> : public detail::basic_promise<void>
{
public:
	//-----------------------------------------------------------------------------
	/// Sets the result to specific value
	//-----------------------------------------------------------------------------
	void set_value()
	{
		const auto set_impl = []() {};
		set_value_and_status(set_impl, detail::value_status::ready);
	}
};

template <typename T>
class future : public detail::basic_future<T>
{
public:
	future() = default;
	future(future&& rhs) noexcept = default;
	future& operator=(future&& rhs) noexcept = default;

	future(const future&) = delete;
	future& operator=(const future&) = delete;

	//-----------------------------------------------------------------------------
	/// The get method waits until the future has a valid
	/// result and (depending on which template is used) retrieves it.
	/// It effectively calls wait() in order to wait for the result.
	/// The behavior is undefined if valid() is false before the call to this
	/// function. Any shared state is released. valid() is false after a call to
	/// this method.
	//-----------------------------------------------------------------------------
	T get()
	{
		this->wait();

		this->rethrow_any_exception();

		auto value = std::move(this->state_->value);

		state_invalidate(this->state_);

		return std::move(*value);
	}

	//-----------------------------------------------------------------------------
	/// Transfers the shared state of *this, if any, to a shared_future object.
	/// Multiple std::shared_future objects may reference the same shared state,
	/// which is not possible with std::future.
	/// After calling share on a std::future, valid() == false.
	//-----------------------------------------------------------------------------
	shared_future<T> share();
};

template <>
class future<void> : public detail::basic_future<void>
{
public:
	future() = default;
	future(future&& rhs) noexcept = default;
	future& operator=(future&& rhs) noexcept = default;

	future(const future&) = delete;
	future& operator=(const future&) = delete;

	//-----------------------------------------------------------------------------
	/// The get method waits until the future has a valid
	/// result and (depending on which template is used) retrieves it.
	/// It effectively calls wait() in order to wait for the result.
	/// The behavior is undefined if valid() is false before the call to this
	/// function. Any shared state is released. valid() is false after a call to
	/// this method.
	//-----------------------------------------------------------------------------
	void get()
	{
		wait();

		rethrow_any_exception();

		state_invalidate(state_);
	}

	//-----------------------------------------------------------------------------
	/// Transfers the shared state of *this, if any, to a shared_future object.
	/// Multiple std::shared_future objects may reference the same shared state,
	/// which is not possible with std::future.
	/// After calling share on a std::future, valid() == false.
	//-----------------------------------------------------------------------------
	shared_future<void> share();
};

//-----------------------------------------------------------------------------
/// The class template 'shared_future' provides a mechanism
/// to access the result of asynchronous operations, similar
/// to 'future', except that multiple threads are allowed to
/// wait for the same shared state. Unlike 'future',
/// which is only moveable (so only one instance can refer
/// to any particular asynchronous result), 'shared_future' is copyable
/// and multiple shared future objects may refer to the same shared state.
/// Access to the same shared state from multiple threads is
/// safe if each thread does it through its own copy of a 'shared_future'
/// object.
//-----------------------------------------------------------------------------
template <typename T>
class shared_future : public detail::basic_future<T>
{
	using base_type = detail::basic_future<T>;

public:
	shared_future() noexcept = default;
	shared_future(const shared_future& sf) = default;

	shared_future(future<T>&& uf) noexcept
		: base_type(std::move(uf))
	{
	}

	shared_future(shared_future&& sf) noexcept = default;
	shared_future& operator=(const shared_future& sf) = default;
	shared_future& operator=(shared_future&& sf) noexcept = default;

	//-----------------------------------------------------------------------------
	/// The get method waits until the future has a valid
	/// result and (depending on which template is used) retrieves it.
	/// It effectively calls wait() in order to wait for the result.
	/// The behavior is undefined if valid() is false before the call to this
	/// function.
	//-----------------------------------------------------------------------------
	const T& get() const
	{
		this->wait();
		this->rethrow_any_exception();

		auto value = this->state_->value;
		return *value;
	}
};

//-----------------------------------------------------------------------------
/// The class template 'shared_future' provides a mechanism
/// to access the result of asynchronous operations, similar
/// to 'future', except that multiple threads are allowed to
/// wait for the same shared state. Unlike 'future',
/// which is only moveable (so only one instance can refer
/// to any particular asynchronous result), 'shared_future' is copyable
/// and multiple shared future objects may refer to the same shared state.
/// Access to the same shared state from multiple threads is
/// safe if each thread does it through its own copy of a 'shared_future'
/// object.
//-----------------------------------------------------------------------------
template <>
class shared_future<void> : public detail::basic_future<void>
{
	using base_type = detail::basic_future<void>;

public:
	shared_future() noexcept = default;
	shared_future(const shared_future& sf) = default;
	shared_future(future<void>&& uf) noexcept
		: base_type(std::move(uf))
	{
	}

	shared_future(shared_future&& sf) noexcept = default;
	shared_future& operator=(const shared_future& sf) = default;
	shared_future& operator=(shared_future&& sf) noexcept = default;

	//-----------------------------------------------------------------------------
	/// The get method waits until the future has a valid
	/// result and (depending on which template is used) retrieves it.
	/// It effectively calls wait() in order to wait for the result.
	/// The behavior is undefined if valid() is false before the call to this
	/// function.
	//-----------------------------------------------------------------------------
	void get() const
	{
		wait();
		rethrow_any_exception();
	}
};

template <typename T>
inline shared_future<T> future<T>::share()
{
	return shared_future<T>(std::move(*this));
}

inline shared_future<void> future<void>::share()
{
	return shared_future<void>(std::move(*this));
}

} // namespace itc
