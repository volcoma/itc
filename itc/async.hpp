#pragma once
#include "detail/apply.hpp"
#include "detail/capture.hpp"
#include "detail/for_each.hpp"
#include "future.hpp"
#include <algorithm>
#include <type_traits>

namespace itc
{

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

void launch(thread::id id, std::launch policy, task func)
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

thread::id get_available_thread()
{
	// Create a new thread to wait on the futures
	// We do not use a thread pool because
	// thread_local initialization must happen
	// as if a new thread is started.
	auto thread = run_thread("future waiter");
	auto id = thread->get_id();
	// detach it and rely that
	// either the call will notify it
	// or the system shutdown will clean it up.
	thread->detach();
	return id;
}

template <typename T, std::enable_if_t<std::is_copy_constructible<T>::value>* = nullptr>
decltype(auto) move_or_copy(T& v)
{
	return std::forward<T>(v);
}

template <typename T, std::enable_if_t<!std::is_copy_constructible<T>::value>* = nullptr>
decltype(auto) move_or_copy(T& v)
{
	return std::move(v);
}

} // namespace detail

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

template <typename T, typename F, typename... Args>
auto then(const shared_future<T>& parent_future, thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
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
	async(waiter_id, std::launch::async, [parent_future, id, policy, task = std::move(task)] {

		parent_future.wait();

		// Launch only if ready.
		if(parent_future.is_ready() && !parent_future.has_error())
		{
			detail::launch(id, policy, std::move(task));
		}

		// We are done. Notify to destroy the
		// executing detached thread.
		notify_for_exit(this_thread::get_id());
	});

	return std::move(future);
}

template <typename T, typename F, typename... Args>
auto then(const shared_future<T>& fut, thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then(fut, id, std::launch::deferred | std::launch::async, std::forward<F>(f),
				std::forward<Args>(args)...);
}

template <typename T, typename F, typename... Args>
auto then(future<T>& fut, thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then(fut.share(), id, policy, std::forward<F>(f), std::forward<Args>(args)...);
}
template <typename T, typename F, typename... Args>
auto then(future<T>&& fut, thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then(fut.share(), id, policy, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename T, typename F, typename... Args>
auto then(future<T>& fut, thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then(fut, id, std::launch::deferred | std::launch::async, std::forward<F>(f),
				std::forward<Args>(args)...);
}

template <typename T, typename F, typename... Args>
auto then(future<T>&& fut, thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return then(fut, id, std::launch::deferred | std::launch::async, std::forward<F>(f),
				std::forward<Args>(args)...);
}

// when_all with variadic arguments
inline future<std::tuple<>> when_all()
{
	return make_ready_future(std::tuple<>());
}

template <typename... Args>
future<std::tuple<std::decay_t<Args>...>> when_all(Args&&... args)
{
	auto tuple_args = std::make_tuple<std::decay_t<Args>...>(detail::move_or_copy(args)...);

	// spawn a detached thread for waiting
	auto waiter_id = detail::get_available_thread();
	return async(waiter_id, [t = std::move(tuple_args)]() mutable {

		for_each(t, [](const auto& waiter) { waiter.wait(); });

		// We are done. Notify to destroy the
		// executing detached thread.
		notify_for_exit(this_thread::get_id());

		return std::move(t);
	});
}
template <class InputIt>
auto when_all(InputIt first, InputIt last)
	-> future<std::vector<typename std::iterator_traits<InputIt>::value_type>>
{
	using container_type = std::vector<typename std::iterator_traits<InputIt>::value_type>;
	container_type args;
	args.reserve(std::distance(first, last));
	while(first != last)
	{
		args.emplace_back(detail::move_or_copy(*first));
		first = std::next(first);
	}

	// spawn a detached thread for waiting
	auto waiter_id = detail::get_available_thread();
	return async(waiter_id, [t = std::move(args)]() mutable {

		for(const auto& waiter : t)
		{
			waiter.wait();
		}

		// We are done. Notify to destroy the
		// executing detached thread.
		notify_for_exit(this_thread::get_id());

		return std::move(t);
	});
}
} // namespace itc
