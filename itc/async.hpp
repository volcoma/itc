#pragma once
#include "detail/apply.hpp"
#include "detail/capture.hpp"
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

thread::id get_available_id()
{
	// create at thread to wait on the futures
	// TODO maybe get it from a pool
	auto thread = run_thread("future waiter");
	auto id = thread->get_id();
	// detach it
	thread->detach();
	return id;
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

template <typename F, typename... Args>
auto async(F&& f, Args&&... args) -> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	auto id = detail::get_available_id();
	return async(id, std::launch::deferred | std::launch::async, std::forward<F>(f),
				 std::forward<Args>(args)...);
}
template <typename T, typename F, typename... Args>
auto when(const shared_future<T>& parent_future, thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	if(parent_future.is_ready())
	{
		return async(id, policy, std::forward<F>(f), std::forward<Args>(args)...);
	}

	auto package = detail::package_task(std::forward<F>(f), std::forward<Args>(args)...);
	auto& future = package.first;
	auto& task = package.second;

	// this call will create a thread so that the future can wait there.
	async(std::launch::async, [parent_future, id, policy, task = std::move(task)] {

		parent_future.wait();

		// Launch only if ready.
		if(parent_future.is_ready())
		{
			detail::launch(id, policy, std::move(task));
		}

		// We are done.
		notify_for_exit(this_thread::get_id());
	});

	return std::move(future);
}

template <typename T, typename F, typename... Args>
auto when(future<T>& fut, thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return when(fut.share(), id, policy, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename T, typename F, typename... Args>
auto when(const shared_future<T>& fut, thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return when(fut, id, std::launch::deferred | std::launch::async, std::forward<F>(f),
				std::forward<Args>(args)...);
}

template <typename T, typename F, typename... Args>
auto when(future<T>& fut, thread::id id, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	return when(fut, id, std::launch::deferred | std::launch::async, std::forward<F>(f),
				std::forward<Args>(args)...);
}

} // namespace itc
