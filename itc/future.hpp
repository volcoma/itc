#pragma once
#include "detail/future_state.hpp"
#include "detail/utility/apply.hpp"
#include "detail/utility/capture.hpp"
#include "detail/utility/invoke.hpp"
#include "thread.h"
#include <future>

namespace itc
{

template <typename T>
class future;
template <typename T>
class shared_future;
template <typename T>
class promise;

template <typename F, typename... Args>
using callable_ret_type = std::result_of_t<std::decay_t<F>(Args...)>;

template <typename F, typename... Args>
using async_ret_type = callable_ret_type<F, Args...>;

template <typename F, typename T>
using then_ret_type = callable_ret_type<F, T>;

//-----------------------------------------------------------------------------
/// The template function async runs the function f a
/// synchronously (potentially in a separate thread )
/// and returns a std::future that will eventually hold
/// the result of that function call. Launch policy async
/// means that the task will be queued regardless if the
/// calling thread is the same as the destination thread.
/// Policy deferred will only queue it if the calling thread
/// and the destination thread are different otherwise it
/// will immediately execute the task.
//-----------------------------------------------------------------------------
template <typename F, typename... Args>
auto async(thread::id id, std::launch policy, F&& f, Args&&... args) -> future<async_ret_type<F, Args...>>;

//-----------------------------------------------------------------------------
/// The template function async runs the function f a
/// synchronously (potentially in a separate thread )
/// and returns a std::future that will eventually hold
/// the result of that function call.
//-----------------------------------------------------------------------------
template <typename F, typename... Args>
auto async(thread::id id, F&& f, Args&&... args) -> future<async_ret_type<F, Args...>>;

//-----------------------------------------------------------------------------
/// produces a future that is ready immediately
/// and holds the given value
//-----------------------------------------------------------------------------
template <typename T>
future<T> make_ready_future(T&& value);

//-----------------------------------------------------------------------------
/// produces a future that is ready immediately
/// and holds the given exception
//-----------------------------------------------------------------------------
template <class T>
future<T> make_exceptional_future(std::exception_ptr ex);

//-----------------------------------------------------------------------------
/// produces a future that is ready immediately
/// and holds the given exception
//-----------------------------------------------------------------------------
template <class T, class E>
future<T> make_exceptional_future(E ex);

namespace detail
{

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
	/// Checks whether the result is ready. If either a value or
	/// exception was set this will return true.
	//-----------------------------------------------------------------------------
	bool is_ready() const
	{
		check_state(state_);

		return state_->ready();
	}

	//-----------------------------------------------------------------------------
	/// Checks whether an error occurred.
	//-----------------------------------------------------------------------------
	bool has_error() const
	{
		check_state(state_);

		return state_->has_error();
	}

	//-----------------------------------------------------------------------------
	/// Waits for the result to become available
	//-----------------------------------------------------------------------------
	void wait() const
	{
		check_state(state_);

		return state_->wait();
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
		check_state(state_);

		return state_->wait_for(timeout_duration);
	}

	template <typename Clock, typename Duration>
	std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) const
	{
		return wait_for(abs_time.time_since_epoch() - Clock::now().time_since_epoch());
	}

protected:
	basic_future(std::shared_ptr<future_state<T>> state)
	{
		state_ = state;
		state_->rethrow_any_exception();
	}

	/// The shared state
	std::shared_ptr<future_state<T>> state_;
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
		if(state_ && !state_->ready())
		{
			abandon();
		}
	}

	//-----------------------------------------------------------------------------
	/// Atomically stores the exception pointer p into the shared state and makes
	/// the state ready. An exception is thrown if there is no shared state or the
	/// shared state already stores a value or exception.
	//-----------------------------------------------------------------------------
	void set_exception(std::exception_ptr p)
	{
		check_state(state_);
		state_->set_exception(std::move(p));
	}

	//-----------------------------------------------------------------------------
	/// Abandons the associated future.
	//-----------------------------------------------------------------------------
	void abandon()
	{
		auto exception = std::make_exception_ptr(std::future_error(std::future_errc::broken_promise));
		set_exception(exception);
	}

	//-----------------------------------------------------------------------------
	/// Returns a future associated with the promised result
	//-----------------------------------------------------------------------------
	future<T> get_future()
	{
		check_retrieved_flag();

		future<T> f;
		f.state_ = state_;
		return f;
	}

protected:
	void check_retrieved_flag()
	{
		check_state(state_);

		if(state_->retrieved.test_and_set())
		{
			throw std::future_error(std::future_errc::future_already_retrieved);
		}
	}

	/// The shared state
	std::shared_ptr<future_state<T>> state_ = std::make_shared<future_state<T>>();
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
		check_state(this->state_);
		this->state_->set_value(std::move(value));
	}
	//-----------------------------------------------------------------------------
	/// Sets the result to specific value
	//-----------------------------------------------------------------------------
	void set_value(const T& value)
	{
		check_state(this->state_);
		this->state_->set_value(value);
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
		check_state(this->state_);
		this->state_->set_value();
	}
};

template <typename T>
class future : public detail::basic_future<T>
{
	using base_type = detail::basic_future<T>;
	using base_type::base_type;

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
		auto state = std::move(this->state_);
		return std::move(state->get_value_assuming_ready());
	}

	//-----------------------------------------------------------------------------
	/// Transfers the shared state of *this, if any, to a shared_future object.
	/// Multiple std::shared_future objects may reference the same shared state,
	/// which is not possible with std::future.
	/// After calling share on a std::future, valid() == false.
	//-----------------------------------------------------------------------------
	shared_future<T> share();

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// After this function returns, valid() is false.
	/// Launch policy asyncmeans that the continuation will be queued
	/// regardless if the calling thread is the same as the destination thread.
	/// Policy deferred will only queue it if the calling thread
	/// and the destination thread are different otherwise it
	/// will immediately execute the continuation when the future is ready.
	//-----------------------------------------------------------------------------
	template <typename F>
	auto then(thread::id id, std::launch policy, F&& f) -> future<then_ret_type<F, future<T>>>;

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// After this function returns, valid() is false.
	//-----------------------------------------------------------------------------
	template <typename F>
	auto then(thread::id id, F&& f) -> future<then_ret_type<F, future<T>>>;
};

template <>
class future<void> : public detail::basic_future<void>
{
	using base_type = detail::basic_future<void>;
	using base_type::base_type;

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
		this->wait();
		auto state = std::move(this->state_);
		state->get_value_assuming_ready();
	}

	//-----------------------------------------------------------------------------
	/// Transfers the shared state of *this, if any, to a shared_future object.
	/// Multiple std::shared_future objects may reference the same shared state,
	/// which is not possible with std::future.
	/// After calling share on a std::future, valid() == false.
	//-----------------------------------------------------------------------------
	shared_future<void> share();

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// After this function returns, valid() is false.
	/// Launch policy asyncmeans that the continuation will be queued
	/// regardless if the calling thread is the same as the destination thread.
	/// Policy deferred will only queue it if the calling thread
	/// and the destination thread are different otherwise it
	/// will immediately execute the continuation when the future is ready.
	//-----------------------------------------------------------------------------
	template <typename F>
	auto then(thread::id id, std::launch policy, F&& f) -> future<then_ret_type<F, future<void>>>;

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// After this function returns, valid() is false.
	//-----------------------------------------------------------------------------
	template <typename F>
	auto then(thread::id id, F&& f) -> future<then_ret_type<F, future<void>>>;
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
	using base_type::base_type;

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
		return this->state_->get_value_assuming_ready();
	}

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// Launch policy asyncmeans that the continuation will be queued
	/// regardless if the calling thread is the same as the destination thread.
	/// Policy deferred will only queue it if the calling thread
	/// and the destination thread are different otherwise it
	/// will immediately execute the continuation when the future is ready.
	//-----------------------------------------------------------------------------
	template <typename F>
	auto then(thread::id id, std::launch policy, F&& f) const -> future<then_ret_type<F, shared_future<T>>>;

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	//-----------------------------------------------------------------------------
	template <typename F>
	auto then(thread::id id, F&& f) const -> future<then_ret_type<F, shared_future<T>>>;
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
	using base_type::base_type;

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
		state_->get_value_assuming_ready();
	}

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// Launch policy asyncmeans that the continuation will be queued
	/// regardless if the calling thread is the same as the destination thread.
	/// Policy deferred will only queue it if the calling thread
	/// and the destination thread are different otherwise it
	/// will immediately execute the continuation when the future is ready.
	//-----------------------------------------------------------------------------
	template <typename F>
	auto then(thread::id id, std::launch policy, F&& f) const
		-> future<then_ret_type<F, shared_future<void>>>;

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	//-----------------------------------------------------------------------------
	template <typename F>
	auto then(thread::id id, F&& f) const -> future<then_ret_type<F, shared_future<void>>>;
};

//-----------------------------------------------------------------------------
/// IMPLEMENTATIONS
//-----------------------------------------------------------------------------
template <typename T>
future<T> make_ready_future(T&& value)
{
	promise<T> prom;
	prom.set_value(std::forward<T>(value));
	return prom.get_future();
}

inline future<void> make_ready_future()
{
	promise<void> prom;
	prom.set_value();
	return prom.get_future();
}

template <class T>
future<T> make_exceptional_future(std::exception_ptr ex)
{
	promise<T> p;
	p.set_exception(ex);
	return p.get_future();
}

template <class T, class E>
future<T> make_exceptional_future(E ex)
{
	promise<T> p;
	p.set_exception(std::make_exception_ptr(ex));
	return p.get_future();
}

template <typename T>
shared_future<T> future<T>::share()
{
	return shared_future<T>(std::move(*this));
}

inline shared_future<void> future<void>::share()
{
	return shared_future<void>(std::move(*this));
}
namespace detail
{
template <typename T, typename F, typename Args>
std::enable_if_t<!std::is_same<T, void>::value> call(promise<T>& p, F&& f, Args&& args)
{
	p.set_value(utility::apply(std::forward<F>(f), std::forward<Args>(args)));
}
template <typename T, typename F, typename Args>
std::enable_if_t<std::is_same<T, void>::value> call(promise<T>& p, F&& f, Args&& args)
{
	utility::apply(std::forward<F>(f), std::forward<Args>(args));
	p.set_value();
}

template <typename T>
struct packaged_task
{
	future<T> callable_future;
	task callable;
};

template <typename F, typename... Args>
auto package_future_task(F&& func, Args&&... args) -> packaged_task<async_ret_type<F, Args...>>
{
	using return_type = async_ret_type<F, Args...>;
	auto prom = promise<return_type>();
	auto fut = prom.get_future();

	auto f = capture(std::forward<F>(func));
	auto p = capture(prom);
	auto params = capture_pack(std::forward<Args>(args)...);
	return {std::move(fut), [f, p, params]() mutable {
				try
				{
					detail::call(p.get(), f.get(), params.get());
				}
				catch(...)
				{
					try
					{
						// store anything thrown in the promise
						p.get().set_exception(std::current_exception());
					}
					catch(...)
					{
					} // set_exception() may throw too
				}
			}};
}

inline void launch(thread::id id, std::launch policy, task& func)
{

	// Here we are not using invoke/run_or_invoke
	// directly so that we can avoid repackaging again
	// thus avoinding extra moves/copies
	if(policy == std::launch::async)
	{
		// invoke(id, func);
		detail::invoke_packaged_task(id, func);
	}
	else
	{
		// run_or_invoke(id, func);
		if(this_thread::get_id() == id)
		{
			// directly call it
			func();
		}
		else
		{
			detail::invoke_packaged_task(id, func);
		}
	}
}
} // namespace detail

template <typename F, typename... Args>
auto async(thread::id id, std::launch policy, F&& f, Args&&... args) -> future<async_ret_type<F, Args...>>
{
	auto package = detail::package_future_task(std::forward<F>(f), std::forward<Args>(args)...);
	auto& future = package.callable_future;
	auto& task = package.callable;

	detail::launch(id, policy, task);

	return std::move(future);
}

template <typename F, typename... Args>
auto async(thread::id id, F&& f, Args&&... args) -> future<async_ret_type<F, Args...>>
{
	return async(id, std::launch::deferred | std::launch::async, std::forward<F>(f),
				 std::forward<Args>(args)...);
}

template <typename T>
template <typename F>
auto future<T>::then(thread::id id, std::launch policy, F&& f) -> future<then_ret_type<F, future<T>>>
{
	detail::check_state(this->state_);

	// invalidate the state
	auto state = std::move(this->state_);
	auto package = detail::package_future_task([f = std::forward<F>(f), state]() {
		future<T> self(state);
		return utility::invoke(f, std::move(self));
	});
	auto& future = package.callable_future;
	auto& task = package.callable;

	state->set_continuation(
		[id, policy, task = std::move(task)]() mutable { detail::launch(id, policy, task); });

	return std::move(future);
}

template <typename T>
template <typename F>
auto future<T>::then(thread::id id, F&& f) -> future<then_ret_type<F, future<T>>>
{
	return then(id, std::launch::async | std::launch::deferred, std::forward<F>(f));
}

template <typename T>
template <typename F>
auto shared_future<T>::then(thread::id id, std::launch policy, F&& f) const
	-> future<then_ret_type<F, shared_future<T>>>
{
	detail::check_state(this->state_);

	// do not invalidate the state
	auto state = this->state_;
	auto package = detail::package_future_task([f = std::forward<F>(f), state]() {
		shared_future<T> self(state);
		return utility::invoke(f, std::move(self));
	});
	auto& future = package.callable_future;
	auto& task = package.callable;

	state->set_continuation(
		[id, policy, task = std::move(task)]() mutable { detail::launch(id, policy, task); });

	return std::move(future);
}

template <typename T>
template <typename F>
auto shared_future<T>::then(thread::id id, F&& f) const -> future<then_ret_type<F, shared_future<T>>>
{
	return then(id, std::launch::async | std::launch::deferred, std::forward<F>(f));
}

template <typename F>
auto future<void>::then(thread::id id, std::launch policy, F&& f) -> future<then_ret_type<F, future<void>>>
{
	detail::check_state(this->state_);

	// invalidate the state
	auto state = std::move(this->state_);
	auto package = detail::package_future_task([f = std::forward<F>(f), state]() {
		future<void> self(state);
		utility::invoke(f, std::move(self));
	});
	auto& future = package.callable_future;
	auto& task = package.callable;

	state->set_continuation(
		[id, policy, task = std::move(task)]() mutable { detail::launch(id, policy, task); });

	return std::move(future);
}

template <typename F>
auto future<void>::then(thread::id id, F&& f) -> future<then_ret_type<F, future<void>>>
{
	return then(id, std::launch::async | std::launch::deferred, std::forward<F>(f));
}

template <typename F>
auto shared_future<void>::then(thread::id id, std::launch policy, F&& f) const
	-> future<then_ret_type<F, shared_future<void>>>
{
	detail::check_state(this->state_);

	// do not invalidate the state
	auto state = this->state_;
	auto package = detail::package_future_task([f = std::forward<F>(f), state]() {
		shared_future<void> self(state);
		utility::invoke(f, std::move(self));
	});
	auto& future = package.callable_future;
	auto& task = package.callable;

	state->set_continuation(
		[id, policy, task = std::move(task)]() mutable { detail::launch(id, policy, task); });

	return std::move(future);
}

template <typename F>
auto shared_future<void>::then(thread::id id, F&& f) const -> future<then_ret_type<F, shared_future<void>>>
{
	return then(id, std::launch::async | std::launch::deferred, std::forward<F>(f));
}
} // namespace itc
