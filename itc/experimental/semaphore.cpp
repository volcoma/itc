#include "semaphore.h"
#include "../thread.h"
#include <algorithm>

namespace itc
{
namespace experimental
{
template <typename ContainerType>
void swap_erase(ContainerType& container, size_t index)
{
	// Swap the element with the back element,
	// except in the case when we're the last element.
	if(index + 1 != container.size())
	{
		std::swap(container[index], container.back());
	}

	// Pop the back of the container, deleting our old element.
	container.pop_back();
}

size_t get_pseudo_random_idx(size_t upper_bound)
{
	auto id = std::this_thread::get_id();
	std::hash<std::thread::id> hasher;
	auto h = (hasher(id) << upper_bound);
	return h % upper_bound;
}

semaphore::semaphore()
{
	waiters_.reserve(16);
}

void semaphore::notify_one() noexcept
{
	auto waiter = [&]() {
		std::lock_guard<std::mutex> lock(mutex_);

		if(!waiters_.empty())
		{
			auto idx = get_pseudo_random_idx(waiters_.size());

			auto waiter = std::move(waiters_[idx]);
			swap_erase(waiters_, idx);

			return waiter;
		}

		return thread_info();
	}();

	if(waiter.id != std::thread::id())
	{
		waiter.flag->store(true);

		notify(waiter.id);
	}
}

void semaphore::notify_all() noexcept
{
	auto waiters = [&]() {
		std::lock_guard<std::mutex> lock(mutex_);

		return std::move(waiters_);
	}();

	for(const auto& waiter : waiters)
	{
		if(waiter.id != std::thread::id())
		{
			waiter.flag->store(true);

			notify(waiter.id);
		}
	}
}

void semaphore::wait(const callback& before_wait, const callback& after_wait) const
{
	auto id = std::this_thread::get_id();

	/// flag is shared_ptr of atomic bool
	/// note: we may be the last reference to the
	/// flag as the notify_functions will set it
	/// and erase it from the container
	auto notified_flag = add_waiter(id);

	while(!(notified_flag->load()))
	{
		if(this_thread::notified_for_exit())
		{
			break;
		}

		if(before_wait)
		{
			before_wait();
		}

		this_thread::wait();

		if(after_wait)
		{
			after_wait();
		}
	}

	remove_waiter(id);
}

std::cv_status semaphore::wait_for_impl(const std::chrono::nanoseconds& timeout_duration,
										const callback& before_wait, const callback& after_wait) const
{
	auto status = std::cv_status::no_timeout;
	auto now = clock::now();
	auto end_time = now + timeout_duration;

	auto id = std::this_thread::get_id();

	/// flag is shared_ptr of atomic bool
	/// note: we may be the last reference to the
	/// flag as the notify_functions will set it
	/// and erase it from the container
	auto notified_flag = add_waiter(id);

	while(!(notified_flag->load()))
	{
		if(now >= end_time)
		{
			status = std::cv_status::timeout;
			break;
		}

		if(this_thread::notified_for_exit())
		{
			break;
		}
		auto time_left = end_time - now;

		if(before_wait)
		{
			before_wait();
		}

		status = this_thread::wait_for(time_left);

		if(after_wait)
		{
			after_wait();
		}

		now = clock::now();
	}

	remove_waiter(id);

	return status;
}

semaphore::notification_flag semaphore::add_waiter(thread::id id) const
{
	std::unique_lock<std::mutex> waiting_lock(mutex_);

	auto it = std::find_if(std::begin(waiters_), std::end(waiters_),
						   [&id](const thread_info& element) { return element.id == id; });
	if(it != std::end(waiters_))
	{
		return it->flag;
	}

	thread_info info;
	info.id = id;
	info.flag = std::make_shared<std::atomic<bool>>(false);
	waiters_.emplace_back(std::move(info));
	return waiters_.back().flag;
}

void semaphore::remove_waiter(thread::id id) const
{
	std::unique_lock<std::mutex> waiting_lock(mutex_);

	waiters_.erase(std::remove_if(std::begin(waiters_), std::end(waiters_),
								  [&id](const thread_info& element) { return element.id == id; }),
				   std::end(waiters_));
}
}
}
