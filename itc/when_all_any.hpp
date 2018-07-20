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

    auto id = this_thread::get_id();
    return head.then(id, [t = std::move(wait_on_tail)](auto h) mutable {
        return std::tuple_cat(std::make_tuple(std::move(h)), t.get());
    });
}

template <class InputIt>
auto when_all(InputIt first, InputIt last)
    -> future<std::vector<typename std::iterator_traits<InputIt>::value_type>>
{
    using container_type = std::vector<typename std::iterator_traits<InputIt>::value_type>;
    if(first == last)
    {
        return make_ready_future<container_type>({});
    }

    auto wait_on_tail = when_all(std::next(first), last);

    auto id = this_thread::get_id();
    return first->then(id, [t = std::move(wait_on_tail)](auto h) mutable {
        auto t_vec = t.get();
        t_vec.emplace_back(std::move(h));
        return std::move(t_vec);
    });
}
} // namespace itc
