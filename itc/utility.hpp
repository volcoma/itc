#pragma once

#include <utility>

namespace itc
{
template <class T>
class move_on_copy_t
{
public:
	move_on_copy_t& operator=(const move_on_copy_t<T>& other) = delete;
	move_on_copy_t& operator=(const move_on_copy_t<T>&& other) = delete;

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
move_on_copy_t<T> make_move_on_copy(T&& value)
{
	return move_on_copy_t<T>(std::forward<T>(value));
}
template <typename T>
move_on_copy_t<T> monc(T& value)
{
	return make_move_on_copy(std::move(value));
}
template <typename T>
move_on_copy_t<T> capture(T& value)
{
	return monc(value);
}
}
