#pragma once
#include "detail/apply.hpp"
#include "detail/capture.hpp"
#include "future.hpp"

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
} // namespace detail

template <typename F, typename... Args>
auto async(thread::id id, std::launch policy, F&& f, Args&&... args)
	-> future<invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
	auto task_pair = detail::package_task(std::forward<F>(f), std::forward<Args>(args)...);

	auto& fut = task_pair.first;
	auto& task = task_pair.second;
	if(policy == std::launch::async)
	{
		invoke(id, std::move(task));
	}
	else
	{
		run_or_invoke(id, std::move(task));
	}
	return std::move(fut);
}

template <class Function, class... Args>
auto async(thread::id id, Function&& func, Args&&... args)
{
	using return_type = invoke_result_t<std::decay_t<Function>, std::decay_t<Args>...>;

	auto prom = promise<return_type>();
	auto fut = prom.get_future();
	auto tuple_args = std::make_tuple<std::decay_t<Args>...>(std::forward<Args>(args)...);

	auto p = capture(prom);
	auto t = capture(tuple_args);
	auto f = capture(func);

	invoke(id, [f, t, p]() mutable { detail::call_and_set_promise_value(p.get(), f.get(), t.get()); });

	return fut;
}
} // namespace itc
