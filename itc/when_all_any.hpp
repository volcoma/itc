#pragma once
#include "future.hpp"
#include <algorithm>
#include <type_traits>

namespace itc
{

inline auto when_all() -> future<std::tuple<>>
{
	return make_ready_future(std::tuple<>());
}

template <class Head, class... Tails>
auto when_all(Head&& head, Tails&&... tails) -> future<std::tuple<std::decay_t<Head>, std::decay_t<Tails>...>>
{
	auto wait_on_tail = when_all(std::move(tails)...);

	return head.then(this_thread::get_id(), [t = std::move(wait_on_tail)](auto h) mutable {
		return std::tuple_cat(std::make_tuple(std::move(h)), t.get());
	});
}

} // namespace itc
