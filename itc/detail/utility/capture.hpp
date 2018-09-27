#pragma once

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace itc
{

template <typename... Args>
class move_on_copy_t
{
public:
	move_on_copy_t& operator=(const move_on_copy_t& other) = delete;
	move_on_copy_t& operator=(move_on_copy_t&& other) = delete;
	using T = decltype(std::make_tuple(std::declval<Args>()...));

	template <typename... U, typename = std::enable_if_t<
								 std::is_same<T, decltype(std::make_tuple(std::declval<U>()...))>::value>>
	move_on_copy_t(U&&... args) noexcept
		: value_(std::make_tuple(std::forward<U>(args)...))
	{
	}

	move_on_copy_t(const move_on_copy_t& other) noexcept
		: value_(std::move(other.value_))
	{
	}

	const T& get() const
	{
		return value_;
	}
	T& get()
	{
		return value_;
	}

private:
	mutable T value_;
};

template <typename... Args>
move_on_copy_t<Args...> capture(Args&&... args) noexcept
{
	return {std::forward<Args>(args)...};
}
} // namespace itc
