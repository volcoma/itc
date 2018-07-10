#pragma once
#include "invoke.hpp"
#include <tuple>
namespace itc
{
namespace detail
{

/*
 * apply implemented as per the C++17 standard specification.
 */
template <class F, class T, std::size_t... I>
constexpr inline decltype(auto) apply(F&& f, T&& t, std::index_sequence<I...>)
{
	(void)t;
	return itc::invoke(std::forward<F>(f), std::get<I>(std::forward<T>(t))...);
}

} // namespace detail

template <class F, class T>
constexpr inline decltype(auto) apply(F&& f, T&& t)
{
	return detail::apply(std::forward<F>(f), std::forward<T>(t),
						 std::make_index_sequence<std::tuple_size<typename std::decay<T>::type>::value>{});
}
}
