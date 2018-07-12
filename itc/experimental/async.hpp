#pragma once
#include "future.hpp"
#include "utility/apply.hpp"

namespace itc
{
namespace experimental
{

namespace detail
{
template <typename T, typename F, typename Tuple>
std::enable_if_t<!std::is_same<T, void>::value> call_and_set_promise_value(promise<T>& p, F&& f, Tuple&& args)
{
    try
	{
        p.set_value(apply(std::forward<F>(f), std::forward<Tuple>(args)));
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
        apply(std::forward<F>(f), std::forward<Tuple>(args));
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
}

template <class Function, class... Args>
auto async(std::thread::id id, Function&& func, Args&&... args)
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
}
}
