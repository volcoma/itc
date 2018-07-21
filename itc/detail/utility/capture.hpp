#pragma once

#include <utility>

namespace itc
{
template <typename T>
class move_on_copy_t
{
public:
	move_on_copy_t& operator=(const move_on_copy_t<T>& other) = delete;
	move_on_copy_t& operator=(move_on_copy_t<T>&& other) = delete;

	move_on_copy_t(T&& value) noexcept
		: value_(std::move(value))
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

template <typename T>
move_on_copy_t<std::decay_t<T>> capture(T& value)
{
    return {std::move(value)};
}

template <typename T>
move_on_copy_t<std::decay_t<T>> capture(T&& value)
{
    return {std::forward<T>(value)};
}

} // namespace itc
