#pragma once
#include "future.hpp"
#include <algorithm>
#include <type_traits>

namespace itc
{
//-----------------------------------------------------------------------------
/// Create a future object that becomes ready when all of the
/// input futures and shared_futures become ready. The behavior
/// is undefined if any input future or shared_future is invalid.
//-----------------------------------------------------------------------------
template <typename... Futures>
auto when_all(Futures&&... futures) -> future<std::tuple<std::decay_t<Futures>...>>;

//-----------------------------------------------------------------------------
/// Create a future object that becomes ready when all of the
/// input futures and shared_futures become ready. The behavior
/// is undefined if any input future or shared_future is invalid.
//-----------------------------------------------------------------------------
template <class InputIt>
auto when_all(InputIt first, InputIt last)
	-> future<std::vector<typename std::iterator_traits<InputIt>::value_type>>;

template <typename Sequence>
struct when_any_result
{
	size_t index;
	Sequence futures;
};
//-----------------------------------------------------------------------------
/// Create a future object that becomes ready when at least one
/// of the input futures and shared_futures become ready.
/// The behavior is undefined if any input future or shared_future is invalid.
//-----------------------------------------------------------------------------
template <typename... Futures>
auto when_any(Futures&&... futures) -> future<when_any_result<std::tuple<std::decay_t<Futures>...>>>;

//-----------------------------------------------------------------------------
/// Create a future object that becomes ready when at least one
/// of the input futures and shared_futures become ready.
/// The behavior is undefined if any input future or shared_future is invalid.
//-----------------------------------------------------------------------------
template <typename InputIt>
auto when_any(InputIt first, InputIt last)
	-> future<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>;

template <typename F, typename... Ts>
void visit_at(std::tuple<Ts...> const& tup, size_t idx, F&& fun);

template <typename F, typename... Ts>
void visit_at(std::tuple<Ts...>& tup, size_t idx, F&& fun);

//-----------------------------------------------------------------------------
/// IMPLEMENTATION
//-----------------------------------------------------------------------------
namespace detail
{

template <size_t I, typename Context>
void when_any_inner_helper(Context context)
{
	using ith_future_type = std::decay_t<decltype(std::get<I>(context->result.futures))>;
	auto id = this_thread::get_id();

	std::get<I>(context->result.futures).then(id, [context](ith_future_type f) {
		std::lock_guard<std::mutex> lock(context->mutex);
		if(!context->ready)
		{
			context->ready = true;
			context->result.index = I;
			std::get<I>(context->result.futures) = std::move(f);
			if(context->processed == context->total && !context->result_moved)
			{
				context->p.set_value(std::move(context->result));
				context->result_moved = true;
			}
		}
	});
}

template <size_t I, size_t S>
struct when_any_helper_struct
{
	template <typename Context, typename... Futures>
	static void apply(const Context& context, std::tuple<Futures...>& t)
	{
		when_any_inner_helper<I>(context);
		++context->processed;
		when_any_helper_struct<I + 1, S>::apply(context, t);
	}
};

template <size_t S>
struct when_any_helper_struct<S, S>
{
	template <typename Context, typename... Futures>
	static void apply(const Context&, std::tuple<Futures...>&)
	{
	}
};

template <typename T, std::enable_if_t<std::is_copy_constructible<T>::value>* = nullptr>
decltype(auto) move_or_copy(T& v)
{
	return std::forward<T>(v);
}

template <typename T, std::enable_if_t<!std::is_copy_constructible<T>::value>* = nullptr>
decltype(auto) move_or_copy(T& v)
{
	return std::move(v);
}
template <size_t I, typename Context>
void fill_result_helper(const Context&)
{
}

template <size_t I, typename Context, typename FirstFuture, typename... Futures>
void fill_result_helper(const Context& context, FirstFuture&& f, Futures&&... fs)
{
	std::get<I>(context->result.futures) = move_or_copy(std::forward<FirstFuture>(f));
	fill_result_helper<I + 1>(context, std::forward<Futures>(fs)...);
}

template <size_t I, typename Context, typename Future>
void when_inner_helper(Context context, Future&& f)
{
	std::get<I>(context->result) = move_or_copy(std::forward<Future>(f));
	auto id = this_thread::get_id();

	std::get<I>(context->result).then(id, [context](typename std::remove_reference<Future>::type f) {
		std::lock_guard<std::mutex> lock(context->mutex);
		++context->ready_futures;
		std::get<I>(context->result) = std::move(f);
		if(context->ready_futures == context->total_futures)
			context->p.set_value(std::move(context->result));
	});
}

template <size_t I, typename Context>
void apply_helper(const Context&)
{
}

template <size_t I, typename Context, typename FirstFuture, typename... Futures>
void apply_helper(const Context& context, FirstFuture&& f, Futures&&... fs)
{
	detail::when_inner_helper<I>(context, std::forward<FirstFuture>(f));
	apply_helper<I + 1>(context, std::forward<Futures>(fs)...);
}

template <size_t I>
struct visit_impl
{
	template <typename T, typename F>
	static void visit(T& tup, size_t idx, F&& fun)
	{
		if(idx == I - 1)
		{
			fun(std::get<I - 1>(tup));
		}
		else
		{
			visit_impl<I - 1>::visit(tup, idx, std::forward<F>(fun));
		}
	}
};

template <>
struct visit_impl<0>
{
	template <typename T, typename F>
	static void visit(T&, size_t, F&&)
	{
        throw std::runtime_error("bad field index");
	}
};

}

template <typename InputIt>
auto when_all(InputIt first, InputIt last)
	-> future<std::vector<typename std::iterator_traits<InputIt>::value_type>>
{
	using result_inner_type = std::vector<typename std::iterator_traits<InputIt>::value_type>;

	struct context
	{
		size_t total_futures = 0;
		size_t ready_futures = 0;
		result_inner_type result;
		std::mutex mutex;
		promise<result_inner_type> p;
	};

	auto shared_context = std::make_shared<context>();
	auto result_future = shared_context->p.get_future();
	shared_context->total_futures = std::distance(first, last);
	shared_context->result.reserve(shared_context->total_futures);
	size_t index = 0;

	for(; first != last; ++first, ++index)
	{
		shared_context->result.emplace_back(std::move(*first));
		shared_context->result[index].then(
			[shared_context, index](typename std::iterator_traits<InputIt>::value_type f) mutable {
				{
					std::lock_guard<std::mutex> lock(shared_context->mutex);
					shared_context->result[index] = std::move(f);
					++shared_context->ready_futures;
					if(shared_context->ready_futures == shared_context->total_futures)
						shared_context->p.set_value(std::move(shared_context->result));
				}
			});
	}

	return result_future;
}

template <typename... Futures>
auto when_all(Futures&&... futures) -> future<std::tuple<std::decay_t<Futures>...>>
{
	using result_inner_type = std::tuple<std::decay_t<Futures>...>;
	struct context
	{
		size_t total_futures;
		size_t ready_futures = 0;
		result_inner_type result;
		promise<result_inner_type> p;
		std::mutex mutex;
	};
	auto shared_context = std::make_shared<context>();
	shared_context->total_futures = sizeof...(futures);
	detail::apply_helper<0>(shared_context, std::forward<Futures>(futures)...);
	return shared_context->p.get_future();
}

template <typename InputIt>
auto when_any(InputIt first, InputIt last)
	-> future<when_any_result<std::vector<typename std::iterator_traits<InputIt>::value_type>>>
{
	using result_inner_type = std::vector<typename std::iterator_traits<InputIt>::value_type>;
	using future_inner_type = when_any_result<result_inner_type>;

	struct context
	{
		size_t total = 0;
		std::atomic<size_t> processed;
		future_inner_type result;
		promise<future_inner_type> p;
		bool ready = false;
		bool result_moved = false;
		std::mutex mutex;
	};

	auto shared_context = std::make_shared<context>();
	auto result_future = shared_context->p.get_future();
	shared_context->processed = 0;
	shared_context->total = std::distance(first, last);
	shared_context->result.futures.reserve(shared_context->total);
	size_t index = 0;

	auto first_copy = first;
	for(; first_copy != last; ++first_copy)
	{
		shared_context->result.futures.emplace_back(std::move(*first_copy));
	}
	auto id = this_thread::get_id();

	for(; first != last; ++first, ++index)
	{
		shared_context->result.futures[index].then(
			id, [shared_context, index](typename std::iterator_traits<InputIt>::value_type f) mutable {
				{
					std::lock_guard<std::mutex> lock(shared_context->mutex);
					if(!shared_context->ready)
					{
						shared_context->result.index = index;
						shared_context->ready = true;
						shared_context->result.futures[index] = std::move(f);
						if(shared_context->processed == shared_context->total &&
						   !shared_context->result_moved)
						{
							shared_context->p.set_value(std::move(shared_context->result));
							shared_context->result_moved = true;
						}
					}
				}
			});
		++shared_context->processed;
	}

	{
		std::lock_guard<std::mutex> lock(shared_context->mutex);
		if(shared_context->ready && !shared_context->result_moved)
		{
			shared_context->p.set_value(std::move(shared_context->result));
			shared_context->result_moved = true;
		}
	}

	return result_future;
}

template <typename... Futures>
auto when_any(Futures&&... futures) -> future<when_any_result<std::tuple<std::decay_t<Futures>...>>>
{
	using result_inner_type = std::tuple<std::decay_t<Futures>...>;
	using future_inner_type = when_any_result<result_inner_type>;

	struct context
	{
		bool ready = false;
		bool result_moved = false;
		size_t total = 0;
		std::atomic<size_t> processed;
		future_inner_type result;
		promise<future_inner_type> p;
		std::mutex mutex;
	};

	auto shared_context = std::make_shared<context>();
	shared_context->processed = 0;
	shared_context->total = sizeof...(futures);

	detail::fill_result_helper<0>(shared_context, std::forward<Futures>(futures)...);
	detail::when_any_helper_struct<0, sizeof...(futures)>::apply(shared_context,
																 shared_context->result.futures);
	{
		std::lock_guard<std::mutex> lock(shared_context->mutex);
		if(shared_context->ready && !shared_context->result_moved)
		{
			shared_context->p.set_value(std::move(shared_context->result));
			shared_context->result_moved = true;
		}
	}
	return shared_context->p.get_future();
}

template <typename F, typename... Ts>
void visit_at(std::tuple<Ts...> const& tup, size_t idx, F&& fun)
{
	detail::visit_impl<sizeof...(Ts)>::visit(tup, idx, std::forward<F>(fun));
}

template <typename F, typename... Ts>
void visit_at(std::tuple<Ts...>& tup, size_t idx, F&& fun)
{
	detail::visit_impl<sizeof...(Ts)>::visit(tup, idx, std::forward<F>(fun));
}
} // namespace itc
