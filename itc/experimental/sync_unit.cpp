#include "sync_unit.h"
#include "../thread.h"
namespace itc
{
namespace experimental
{
void sync_unit::notify_one() noexcept
{
	std::unique_lock<std::mutex> lock(mutex_);
	notify_one_impl(lock);
}

void sync_unit::notify_all() noexcept
{
	std::unique_lock<std::mutex> lock(mutex_);
	while(notify_one_impl(lock) > 0)
	{
	}
}

void sync_unit::wait(const callback& before_wait, const callback& after_wait) const
{
	auto id = std::this_thread::get_id();
	std::unique_lock<std::mutex> waiting_lock(mutex_);
	auto& notified = add_to_waiters(id);

	while(!notified)
	{
		if(this_thread::notified_for_exit())
		{
			break;
		}
		waiting_lock.unlock();
		if(before_wait)
		{
			before_wait();
		}

		this_thread::wait_for_event();

		if(after_wait)
		{
			after_wait();
		}
		waiting_lock.lock();
	}

	waiting_threads_.erase(id);
	waiting_lock.unlock();
}

void sync_unit::wait_for_impl(const std::chrono::nanoseconds& timeout_duration, const callback& before_wait,
							  const callback& after_wait) const
{
	auto now = clock::now();
	auto end_time = now + timeout_duration;

	auto id = std::this_thread::get_id();
	std::unique_lock<std::mutex> waiting_lock(mutex_);
	auto& notified = add_to_waiters(id);

	while(!notified && now < end_time)
	{
		if(this_thread::notified_for_exit())
		{
			break;
		}
		auto time_left = end_time - now;

		waiting_lock.unlock();
		if(before_wait)
		{
			before_wait();
		}

		this_thread::wait_for_event(time_left);

		if(after_wait)
		{
			after_wait();
		}

		waiting_lock.lock();

		now = clock::now();
	}

	waiting_threads_.erase(id);
	waiting_lock.unlock();
}

bool& sync_unit::add_to_waiters(thread::id id) const
{
	auto inserted = waiting_threads_.emplace(id, false);
	auto& it = inserted.first;
	return it->second;
}

size_t sync_unit::notify_one_impl(std::unique_lock<std::mutex>& lock) noexcept
{
	// TODO this impl is inefficent
	std::thread::id id;

	bool notified_one = false;
	size_t total_notified = 0;
	for(auto& waiter : waiting_threads_)
	{
		bool& notified = waiter.second;
		if(!notified)
		{
			if(!notified_one)
			{
				id = waiter.first;
				notified = true;
				total_notified++;
				notified_one = true;
			}
		}
		else
		{
			total_notified++;
		}
	}

	if(id != std::thread::id())
	{
		lock.unlock();
		notify(id);
		lock.lock();
	}

	return waiting_threads_.size() - total_notified;
}
}
}
