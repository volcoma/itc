#pragma once
#include "detail/utility/apply.hpp"
#include "detail/utility/capture.hpp"
#include "detail/utility/for_each.hpp"
#include "future.hpp"
#include <algorithm>
#include <type_traits>

namespace itc
{

namespace detail
{

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
	auto args_wrap = capture(tuple_args);
	return async(waiter_id, [args_wrap]() mutable {

		utility::for_each(args_wrap.get(), [](const auto& waiter) { waiter.wait(); });

		// We are done. Notify to destroy the
		// executing detached thread.
		notify_for_exit(this_thread::get_id());

		return std::move(args_wrap.get());
	});
}
template <class InputIt>
auto when_all(InputIt first, InputIt last)
	-> future<std::vector<typename std::iterator_traits<InputIt>::value_type>>
{
	using container_type = std::vector<typename std::iterator_traits<InputIt>::value_type>;
	container_type args;
	if(first == last)
	{
		return args;
	}
	args.reserve(std::distance(first, last));
	while(first != last)
	{
		args.emplace_back(detail::move_or_copy(*first));
		first = std::next(first);
	}

	// spawn a detached thread for waiting
	auto waiter_id = detail::get_available_thread();
	auto args_wrap = capture(args);
	return async(waiter_id, [args_wrap]() mutable {

		for(const auto& waiter : args_wrap.get())
		{
			waiter.wait();
		}

		// We are done. Notify to destroy the
		// executing detached thread.
		notify_for_exit(this_thread::get_id());

		return std::move(args_wrap.get());
	});
}

} // namespace itc
