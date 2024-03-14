#pragma once

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace itc
{
template<class T>
struct unwrap_refwrapper
{
    using type = T;
};

template<class T>
struct unwrap_refwrapper<std::reference_wrapper<T>>
{
    using type = T&;
};

template<class T>
using special_decay_t = typename unwrap_refwrapper<typename std::decay<T>::type>::type;

template<typename... Args>
class move_on_copy_t
{
public:
    move_on_copy_t& operator=(const move_on_copy_t& other) = delete;
    move_on_copy_t& operator=(move_on_copy_t&& other) = delete;
    using T = std::tuple<special_decay_t<Args>...>;

    template<typename... U, typename = std::enable_if_t<std::is_same<T, std::tuple<special_decay_t<U>...>>::value>>
    move_on_copy_t(U&&... args) noexcept : value_(std::make_tuple(std::forward<U>(args)...))
    {
    }

    move_on_copy_t(const move_on_copy_t& other) noexcept : value_(std::move(other.value_))
    {
    }

    auto get() const -> const T&
    {
        return value_;
    }
    auto get() -> T&
    {
        return value_;
    }

private:
    mutable T value_;
};

template<typename... Args>
auto capture(Args&&... args) noexcept -> move_on_copy_t<Args...>
{
    return {std::forward<Args>(args)...};
}

} // namespace itc
