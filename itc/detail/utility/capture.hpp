#pragma once

#include <tuple>
#include <utility>
#include <functional>
#include <type_traits>
#include <utility>


namespace itc
{

template <typename T>
class move_on_copy_t
{
public:
	move_on_copy_t& operator=(const move_on_copy_t& other) = delete;
	move_on_copy_t& operator=(move_on_copy_t&& other) = delete;

	template <typename U, typename = std::enable_if_t<!std::is_same<std::decay_t<U>, move_on_copy_t>::value>>
	move_on_copy_t(U&& value) noexcept
		: value_(std::forward<U>(value))
	{
	}

	move_on_copy_t(const move_on_copy_t& other) noexcept
		: value_(std::move(other.value_))
	{
	}
	move_on_copy_t(move_on_copy_t&& other) noexcept
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

template <typename T>
move_on_copy_t<std::decay_t<T>> capture(T&& value) noexcept
{
	return {std::forward<T>(value)};
}

} // namespace itc
