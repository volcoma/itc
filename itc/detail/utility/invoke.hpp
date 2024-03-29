#pragma once
#include <cstddef>
#include <type_traits>
#include <utility>

namespace itc
{
template<typename... Args>
inline void ignore(Args&&... /*unused*/)
{
}
namespace utility
{
namespace detail
{
template<typename T>
struct is_reference_wrapper : std::false_type
{
};
template<typename U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type
{
};
/*
 * invoke implemented as per the C++17 standard specification.
 */
template<typename B, typename T, typename D, typename... Args>
constexpr inline auto invoke_impl(T B::*f, D&& d, Args&&... args) noexcept(noexcept((std::forward<D>(d).*
                                                                                     f)(std::forward<Args>(args)...)))
    -> typename std::enable_if<std::is_function<T>::value && std::is_base_of<B, typename std::decay<D>::type>::value,
                               decltype((std::forward<D>(d).*f)(std::forward<Args>(args)...))>::type
{
    return (std::forward<D>(d).*f)(std::forward<Args>(args)...);
}

template<typename B, typename T, typename R, typename... Args>
constexpr inline auto invoke_impl(T B::*f,
                                  R&& r,
                                  Args&&... args) noexcept(noexcept((r.get().*f)(std::forward<Args>(args)...))) ->
    typename std::enable_if<std::is_function<T>::value && is_reference_wrapper<typename std::decay<R>::type>::value,
                            decltype((r.get().*f)(std::forward<Args>(args)...))>::type
{
    return (r.get().*f)(std::forward<Args>(args)...);
}

template<typename B, typename T, typename P, typename... Args>
constexpr inline auto invoke_impl(T B::*f, P&& p, Args&&... args) noexcept(noexcept(((*std::forward<P>(p)).*
                                                                                     f)(std::forward<Args>(args)...)))
    ->
    typename std::enable_if<std::is_function<T>::value && !is_reference_wrapper<typename std::decay<P>::type>::value &&
                                !std::is_base_of<B, typename std::decay<P>::type>::value,
                            decltype(((*std::forward<P>(p)).*f)(std::forward<Args>(args)...))>::type
{
    return ((*std::forward<P>(p)).*f)(std::forward<Args>(args)...);
}

template<typename B, typename T, typename D>
constexpr inline auto invoke_impl(T B::*m, D&& d) noexcept(noexcept(std::forward<D>(d).*m)) ->
    typename std::enable_if<!std::is_function<T>::value && std::is_base_of<B, typename std::decay<D>::type>::value,
                            decltype(std::forward<D>(d).*m)>::type
{
    return std::forward<D>(d).*m;
}

template<typename B, typename T, typename R>
constexpr inline auto invoke_impl(T B::*m, R&& r) noexcept(noexcept(r.get().*m)) ->
    typename std::enable_if<!std::is_function<T>::value && is_reference_wrapper<typename std::decay<R>::type>::value,
                            decltype(r.get().*m)>::type
{
    return r.get().*m;
}

template<typename B, typename T, typename P>
constexpr inline auto invoke_impl(T B::*m, P&& p) noexcept(noexcept((*std::forward<P>(p)).*m)) ->
    typename std::enable_if<!std::is_function<T>::value && !is_reference_wrapper<typename std::decay<P>::type>::value &&
                                !std::is_base_of<B, typename std::decay<P>::type>::value,
                            decltype((*std::forward<P>(p)).*m)>::type
{
    return (*std::forward<P>(p)).*m;
}

template<typename Callable, typename... Args>
constexpr inline auto invoke_impl(Callable&& c, Args&&... args) noexcept(
    noexcept(std::forward<Callable>(c)(std::forward<Args>(args)...)))
    -> decltype(std::forward<Callable>(c)(std::forward<Args>(args)...))
{
    return std::forward<Callable>(c)(std::forward<Args>(args)...);
}

} // namespace detail

template<typename F, typename... Args>
constexpr inline auto invoke(F&& f, Args&&... args) noexcept(noexcept(detail::invoke_impl(std::forward<F>(f),
                                                                                          std::forward<Args>(args)...)))
    -> decltype(detail::invoke_impl(std::forward<F>(f), std::forward<Args>(args)...))
{
    return detail::invoke_impl(std::forward<F>(f), std::forward<Args>(args)...);
}

// Conforming C++14 implementation (is also a valid C++11 implementation):
namespace detail
{
template<typename T>
struct identity
{
    using type = T;
};
template<typename...>
struct voider : identity<void>
{
};

template<typename... Ts>
using void_t = typename voider<Ts...>::type;

template<typename F, typename... Args>
using invoke_result_ = decltype(utility::invoke(std::declval<F>(), std::declval<Args>()...));

template<typename AlwaysVoid, typename, typename...>
struct invoke_result
{
};
template<typename F, typename... Args>
struct invoke_result<void_t<invoke_result_<F, Args...>>, F, Args...>
{
    using type = invoke_result_<F, Args...>;
};
} // namespace detail

template<typename>
struct result_of;
template<typename F, typename... ArgTypes>
struct result_of<F(ArgTypes...)> : detail::invoke_result<void, F, ArgTypes...>
{
};

template<typename F, typename... Args>
using result_of_t = typename result_of<F(Args...)>::type;

template<typename F, typename... ArgTypes>
struct invoke_result : detail::invoke_result<void, F, ArgTypes...>
{
};

template<typename F, typename... Args>
using invoke_result_t = typename invoke_result<F, Args...>::type;
} // namespace utility
} // namespace itc
