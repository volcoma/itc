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
typename std::enable_if<!std::is_same<T, void>::value>::type set_promise_value(promise<T>& p, F&& f,
																			   Tuple&& args)
{
	p.set_value(apply(std::forward<F>(f), std::forward<Tuple>(args)));
}
template <typename T, typename F, typename Tuple>
typename std::enable_if<std::is_same<T, void>::value>::type set_promise_value(promise<T>& p, F&& f,
																			  Tuple&& args)
{
	apply(std::forward<F>(f), std::forward<Tuple>(args));
	p.set_value();
}
}

template <class Function, class... Args>
future<invoke_result_t<std::decay_t<Function>, std::decay_t<Args>...>> async(std::thread::id id,
																			 Function&& func, Args&&... args)
{
	using return_type = invoke_result_t<std::decay_t<Function>, std::decay_t<Args>...>;

	promise<return_type> promise;
	auto fut = promise.get_future();
	auto tuple_args = std::make_tuple<std::decay_t<Args>...>(std::forward<Args>(args)...);
	auto p = capture(promise);
	auto t = capture(tuple_args);
	auto f = capture(func);
	invoke(id, [f, t, p]() mutable { detail::set_promise_value(p.get(), f.get(), t.get()); });

	return fut;
}
}
}
