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
