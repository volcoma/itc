#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>

namespace itc
{
template <typename... Args>
inline void ignore(Args&&... /*unused*/)
{
}
namespace utility
{
namespace detail
{
template <typename T>
struct is_reference_wrapper : std::false_type
{
};
template <typename U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type
{
};
/*
 * invoke implemented as per the C++17 standard specification.
 */
template <typename B, typename T, typename D, typename... Args>
constexpr inline auto
invoke_impl(T B::*f, D&& d,
			Args&&... args) noexcept(noexcept((std::forward<D>(d).*f)(std::forward<Args>(args)...))) ->
	typename std::enable_if<std::is_function<T>::value &&
								std::is_base_of<B, typename std::decay<D>::type>::value,
							decltype((std::forward<D>(d).*f)(std::forward<Args>(args)...))>::type
{
	return (std::forward<D>(d).*f)(std::forward<Args>(args)...);
}

template <typename B, typename T, typename R, typename... Args>
constexpr inline auto
invoke_impl(T B::*f, R&& r, Args&&... args) noexcept(noexcept((r.get().*f)(std::forward<Args>(args)...))) ->
	typename std::enable_if<std::is_function<T>::value &&
								is_reference_wrapper<typename std::decay<R>::type>::value,
							decltype((r.get().*f)(std::forward<Args>(args)...))>::type
{
	return (r.get().*f)(std::forward<Args>(args)...);
}

template <typename B, typename T, typename P, typename... Args>
constexpr inline auto
invoke_impl(T B::*f, P&& p,
			Args&&... args) noexcept(noexcept(((*std::forward<P>(p)).*f)(std::forward<Args>(args)...))) ->
	typename std::enable_if<std::is_function<T>::value &&
								!is_reference_wrapper<typename std::decay<P>::type>::value &&
								!std::is_base_of<B, typename std::decay<P>::type>::value,
							decltype(((*std::forward<P>(p)).*f)(std::forward<Args>(args)...))>::type
{
	return ((*std::forward<P>(p)).*f)(std::forward<Args>(args)...);
}

template <typename B, typename T, typename D>
constexpr inline auto invoke_impl(T B::*m, D&& d) noexcept(noexcept(std::forward<D>(d).*m)) ->
	typename std::enable_if<!std::is_function<T>::value &&
								std::is_base_of<B, typename std::decay<D>::type>::value,
							decltype(std::forward<D>(d).*m)>::type
{
	return std::forward<D>(d).*m;
}

template <typename B, typename T, typename R>
constexpr inline auto invoke_impl(T B::*m, R&& r) noexcept(noexcept(r.get().*m)) ->
	typename std::enable_if<!std::is_function<T>::value &&
								is_reference_wrapper<typename std::decay<R>::type>::value,
							decltype(r.get().*m)>::type
{
	return r.get().*m;
}

template <typename B, typename T, typename P>
constexpr inline auto invoke_impl(T B::*m, P&& p) noexcept(noexcept((*std::forward<P>(p)).*m)) ->
	typename std::enable_if<!std::is_function<T>::value &&
								!is_reference_wrapper<typename std::decay<P>::type>::value &&
								!std::is_base_of<B, typename std::decay<P>::type>::value,
							decltype((*std::forward<P>(p)).*m)>::type
{
	return (*std::forward<P>(p)).*m;
}

template <typename Callable, typename... Args>
constexpr inline auto
invoke_impl(Callable&& c,
			Args&&... args) noexcept(noexcept(std::forward<Callable>(c)(std::forward<Args>(args)...)))
	-> decltype(std::forward<Callable>(c)(std::forward<Args>(args)...))
{
	return std::forward<Callable>(c)(std::forward<Args>(args)...);
}

} // namespace detail

template <typename F, typename... Args>
constexpr inline auto invoke(F&& f, Args&&... args) noexcept(
	noexcept(detail::invoke_impl(std::forward<F>(f), std::forward<Args>(args)...)))
	-> decltype(detail::invoke_impl(std::forward<F>(f), std::forward<Args>(args)...))
{
	return detail::invoke_impl(std::forward<F>(f), std::forward<Args>(args)...);
}

// Conforming C++14 implementation (is also a valid C++11 implementation):
namespace detail
{
///////////////////////////////////////////////////////////////////////////

template <typename T, typename Enable = void>
struct invoke_result_impl
{
};

template <typename F, typename... Ts>
struct invoke_result_impl<F(Ts...), decltype((void)invoke(std::declval<F>(), std::declval<Ts>()...))>
{
	using type = decltype(invoke(std::declval<F>(), std::declval<Ts>()...));
};
} // namespace detail

//! template <class Fn, class... ArgTypes> struct invoke_result;
//!
//! - _Comments_: If the expression `INVOKE(std::declval<Fn>(),
//!   std::declval<ArgTypes>()...)` is well-formed when treated as an
//!   unevaluated operand, the member typedef `type` names the type
//!   `decltype(INVOKE(std::declval<Fn>(), std::declval<ArgTypes>()...))`;
//!   otherwise, there shall be no member `type`. Access checking is
//!   performed as if in a context unrelated to `Fn` and `ArgTypes`. Only
//!   the validity of the immediate context of the expression is considered.
//!
//! - _Preconditions_: `Fn` and all types in the template parameter pack
//!   `ArgTypes` are complete types, _cv_ `void`, or arrays of unknown
//!   bound.
template <typename Fn, typename... ArgTypes>
struct invoke_result : detail::invoke_result_impl<Fn && (ArgTypes && ...)>
{
};

//! template <class Fn, class... ArgTypes>
//! using invoke_result_t = typename invoke_result<Fn, ArgTypes...>::type;
template <typename Fn, typename... ArgTypes>
using invoke_result_t = typename invoke_result<Fn, ArgTypes...>::type;
} // namespace utility
} // namespace itc
