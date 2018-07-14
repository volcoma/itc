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
template <typename F, typename T, std::size_t... I>
constexpr inline decltype(auto) apply(F&& f, T&& t, std::index_sequence<I...>)
{
	(void)t;
	return itc::invoke(std::forward<F>(f), std::get<I>(std::forward<T>(t))...);
}

} // namespace detail

template <typename F, typename T>
constexpr inline decltype(auto) apply(F&& f, T&& t)
{
	return detail::apply(std::forward<F>(f), std::forward<T>(t),
						 std::make_index_sequence<std::tuple_size<std::decay_t<T>>::value>{});
}

} // namespace itc
