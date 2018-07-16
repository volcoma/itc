#pragma once
#include "detail/apply.hpp"
#include "detail/capture.hpp"
#include "detail/for_each.hpp"
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

//-----------------------------------------------------------------------------
/// The template function async runs the function f a
/// synchronously (potentially in a separate thread )
/// and returns a std::future that will eventually hold
/// the result of that function call.
//-----------------------------------------------------------------------------
template <typename F, typename... Args>
auto async(thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

//-----------------------------------------------------------------------------
/// The template function async runs the function f a
/// synchronously (potentially in a separate thread )
/// and returns a std::future that will eventually hold
/// the result of that function call.
//-----------------------------------------------------------------------------
template <typename F, typename... Args>
auto async(thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;


namespace detail
{
template <typename T, typename F, typename... Args>
auto then_impl(const shared_future<T>& parent_future, thread::id id, std::launch policy, F&& f,
			   Args&&... args) -> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;


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

	/// These are here so that constructors of both
	/// future and promise can remain noexcept
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
	/// Checks whether the result is ready. If either a value or
	/// exception was set this will return true.
	//-----------------------------------------------------------------------------
	bool is_ready() const
	{
		state_check(state_);

		return state_->status != value_status::not_set;
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

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// After this function returns, valid() is false.
	//-----------------------------------------------------------------------------
	template <typename F, typename... Args>
	auto then(thread::id id, std::launch policy, F&& f, Args&&... args)
		-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// After this function returns, valid() is false.
	//-----------------------------------------------------------------------------
	template <typename F, typename... Args>
	auto then(thread::id id, F&& f, Args&&... args)
		-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;
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

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// After this function returns, valid() is false.
	//-----------------------------------------------------------------------------
	template <typename F, typename... Args>
	auto then(thread::id id, std::launch policy, F&& f, Args&&... args)
		-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	/// After this function returns, valid() is false.
	//-----------------------------------------------------------------------------
	template <typename F, typename... Args>
	auto then(thread::id id, F&& f, Args&&... args)
		-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;
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

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	//-----------------------------------------------------------------------------
	template <typename F, typename... Args>
	auto then(thread::id id, std::launch policy, F&& f, Args&&... args) const
		-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	//-----------------------------------------------------------------------------
	template <typename F, typename... Args>
	auto then(thread::id id, F&& f, Args&&... args) const
		-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;
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

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	//-----------------------------------------------------------------------------
	template <typename F, typename... Args>
	auto then(thread::id id, std::launch policy, F&& f, Args&&... args) const
		-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

	//-----------------------------------------------------------------------------
	/// Attach the continuation func to *this. The behavior is undefined
	/// if *this has no associated shared state (i.e., valid() == false).
	//-----------------------------------------------------------------------------
	template <typename F, typename... Args>
	auto then(thread::id id, F&& f, Args&&... args) const
		-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;
};

//-----------------------------------------------------------------------------
/// produces a future that is ready immediately
/// and holds the given value
//-----------------------------------------------------------------------------
template <typename T>
inline future<T> make_ready_future(T&& value)
{
	promise<T> prom;
	prom.set_value(std::forward<T>(value));
	return prom.get_future();
}

//-----------------------------------------------------------------------------
/// produces a future that is ready immediately
/// and holds the given value
//-----------------------------------------------------------------------------
inline future<void> make_ready_future()
{
	promise<void> prom;
	prom.set_value();
	return prom.get_future();
}

//-----------------------------------------------------------------------------
/// IMPLEMENTATIONS
//-----------------------------------------------------------------------------
template <typename T>
inline shared_future<T> future<T>::share()
{
	return shared_future<T>(std::move(*this));
}

inline shared_future<void> future<void>::share()
{
	return shared_future<void>(std::move(*this));
}
namespace detail
{
template <typename T, typename F, typename Tuple>
std::enable_if_t<!std::is_same<T, void>::value> call_and_set_promise_value(promise<T>& p, F&& f, Tuple&& args)
{
	try
	{
		p.set_value(itc::apply(std::forward<F>(f), std::forward<Tuple>(args)));
	}
	catch(...)
	{
		try
		{
			// store anything thrown in the promise
			p.set_exception(std::current_exception());
		}
		catch(...)
		{
		} // set_exception() may throw too
	}
}
template <typename T, typename F, typename Tuple>
std::enable_if_t<std::is_same<T, void>::value> call_and_set_promise_value(promise<T>& p, F&& f, Tuple&& args)
{
	try
	{
		itc::apply(std::forward<F>(f), std::forward<Tuple>(args));
		p.set_value();
	}
	catch(...)
	{
		try
		{
			// store anything thrown in the promise
			p.set_exception(std::current_exception());
		}
		catch(...)
		{
		} // set_exception() may throw too
	}
}

template <typename F, typename... Args>
auto package_task(F&& func, Args&&... args)
	-> std::pair<future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>, task>
{
	using return_type = invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

	auto prom = promise<return_type>();
	auto fut = prom.get_future();
	auto tuple_args = std::make_tuple<std::decay_t<Args>...>(std::forward<Args>(args)...);

	auto p = capture(prom);
	auto t = capture(tuple_args);
	auto f = capture(func);

	return std::make_pair(std::move(fut), [f, t, p]() mutable {
		detail::call_and_set_promise_value(p.get(), f.get(), t.get());
	});
}

inline void launch(thread::id id, std::launch policy, task func)
{
	if(policy == std::launch::async)
	{
		invoke(id, std::move(func));
	}
	else
	{
		run_or_invoke(id, std::move(func));
	}
}

inline thread::id get_available_thread()
{
	/// Create a new thread to wait on the futures
	/// We do not use a thread pool because
	/// thread_local initialization must happen
	/// as if a new thread is started.
	auto thread = run_thread("future waiter");
	auto id = thread->get_id();
	/// detach it and rely that
	/// either the call will notify it
	/// or the system shutdown will clean it up.
	thread->detach();
	return id;
}

template <typename T, typename F, typename... Args>
auto then_impl(const shared_future<T>& parent_future, thread::id id, std::launch policy, F&& f,
			   Args&&... args) -> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	if(parent_future.is_ready() && !parent_future.has_error())
	{
		return async(id, policy, std::forward<F>(f), std::forward<Args>(args)...);
	}

	auto package = detail::package_task(std::forward<F>(f), std::forward<Args>(args)...);
	auto& future = package.first;
	auto& task = package.second;

	// spawn a detached thread for waiting
	auto waiter_id = detail::get_available_thread();
	detail::launch(waiter_id, std::launch::async, [parent_future, id, policy, task = std::move(task)] {

		parent_future.wait();

		// Launch only if ready.
		if(parent_future.is_ready() && !parent_future.has_error())
		{
			detail::launch(id, policy, std::move(task));
		}
		else
		{
			// TODO handle exception propagation
		}

		// We are done. Notify to destroy the
		// executing detached thread.
		notify_for_exit(this_thread::get_id());
	});

	return std::move(future);
}
}

template <typename F, typename... Args>
auto async(thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	auto package = detail::package_task(std::forward<F>(f), std::forward<Args>(args)...);
	auto& future = package.first;
	auto& task = package.second;

	detail::launch(id, policy, std::move(task));

	return std::move(future);
}

template <typename F, typename... Args>
auto async(thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return async(id, std::launch::deferred | std::launch::async, std::forward<F>(f),
				 std::forward<Args>(args)...);
}

template <typename T>
template <typename F, typename... Args>
inline auto future<T>::then(thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then_impl(share(), id, policy, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename T>
template <typename F, typename... Args>
inline auto future<T>::then(thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then_impl(share(), id, std::launch::async | std::launch::deferred, std::forward<F>(f),
					 std::forward<Args>(args)...);
}

template <typename T>
template <typename F, typename... Args>
inline auto shared_future<T>::then(thread::id id, std::launch policy, F&& f, Args&&... args) const
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then_impl(*this, id, policy, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename T>
template <typename F, typename... Args>
inline auto shared_future<T>::then(thread::id id, F&& f, Args&&... args) const
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then_impl(*this, id, std::launch::async | std::launch::deferred, std::forward<F>(f),
					 std::forward<Args>(args)...);
}

template <typename F, typename... Args>
inline auto future<void>::then(thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then_impl(share(), id, policy, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename F, typename... Args>
inline auto future<void>::then(thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then_impl(share(), id, std::launch::async | std::launch::deferred, std::forward<F>(f),
					 std::forward<Args>(args)...);
}

template <typename F, typename... Args>
inline auto shared_future<void>::then(thread::id id, std::launch policy, F&& f, Args&&... args) const
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then_impl(*this, id, policy, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename F, typename... Args>
inline auto shared_future<void>::then(thread::id id, F&& f, Args&&... args) const
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then_impl(*this, id, std::launch::async | std::launch::deferred, std::forward<F>(f),
					 std::forward<Args>(args)...);
}
} // namespace itc
