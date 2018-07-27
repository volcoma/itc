#pragma once

#include <tuple>
#include <utility>

namespace itc
{

template <typename T>
class move_on_copy_t
{
public:
	move_on_copy_t& operator=(const move_on_copy_t& other) = delete;
	move_on_copy_t& operator=(move_on_copy_t&& other) = delete;

	move_on_copy_t(T&& value) noexcept
		: value_(std::forward<T>(value))
	{
	}
	move_on_copy_t(T& value) noexcept
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
move_on_copy_t<std::decay_t<T>> capture(T&& value)
{
	return {std::forward<T>(value)};
}

template <typename... Args>
class move_on_copy_pack_t
{
public:
	using T = std::tuple<std::decay_t<Args>...>;
	move_on_copy_pack_t& operator=(const move_on_copy_pack_t& other) = delete;
	move_on_copy_pack_t& operator=(move_on_copy_pack_t&& other) = delete;

	move_on_copy_pack_t(Args&&... value) noexcept
		: value_(std::forward<Args>(value)...)
	{
	}
	move_on_copy_pack_t(const move_on_copy_pack_t& other) noexcept
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

template <typename... T>
move_on_copy_pack_t<T...> capture_pack(T&&... value)
{
	return {std::forward<T>(value)...};
}

} // namespace itc
