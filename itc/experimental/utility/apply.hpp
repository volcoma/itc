#pragma once
#include <tuple>
#include <type_traits>
namespace itc
{
namespace experimental
{

namespace detail
{

template <std::size_t... Is>
auto index_over(std::index_sequence<Is...>)
{
	return [](auto&& f) -> decltype(auto) {
		return decltype(f)(f)(std::integral_constant<std::size_t, Is>{}...);
	};
}
template <std::size_t N>
auto index_upto(std::integral_constant<std::size_t, N> = {})
{
	return index_over(std::make_index_sequence<N>{});
}
template <class T>
constexpr auto tuple_size_v = std::tuple_size<T>::value;
}

template <class F, class Tuple>
decltype(auto) apply(F&& f, Tuple&& tup)
{
	auto indexer = detail::index_upto<detail::tuple_size_v<std::remove_reference_t<Tuple>>>();
	return indexer([&](auto... Is) -> decltype(auto) {
		return std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(tup))...);
	});
}
}
}
