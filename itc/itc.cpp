#include "itc.h"
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace itc
{
struct context
{
	std::mutex tasks_mutex;
	std::vector<task> tasks;

	std::vector<task> processing_tasks;
	std::size_t processing_idx = 0;

	std::condition_variable wakeup_event;
	std::atomic_bool wakeup = {false};

	std::atomic_bool running = {true};
};

struct shared_data
{
	std::mutex mutex;
	std::map<std::thread::id, std::shared_ptr<context>> contexts;
	std::thread::id main_thread_id = std::this_thread::get_id();
};

static shared_data global_data;
thread_local static context* local_data = nullptr;

void set_local_data(context* ctx)
{
	local_data = ctx;
}

std::shared_ptr<context> register_thread_impl(std::thread::id id)
{
	std::unique_lock<std::mutex> lock(global_data.mutex);
	auto it = global_data.contexts.find(id);
	if(it != global_data.contexts.end())
	{
		return it->second;
	}

	auto ctx = std::make_shared<context>();
	global_data.contexts.emplace(id, ctx);
	return ctx;
}

void unregister_thread_impl(std::thread::id id)
{
	std::lock_guard<std::mutex> lock(global_data.mutex);
	auto it = global_data.contexts.find(id);
	if(it == global_data.contexts.end())
	{
		return;
	}

	auto ctx = it->second;

	std::lock_guard<std::mutex> local_lock(ctx->tasks_mutex);

	global_data.contexts.erase(id);
}

bool has_tasks_to_process(const context& ctx)
{
	return ctx.processing_idx < ctx.processing_tasks.size();
}

bool prepare_tasks(context& ctx)
{
	if(!has_tasks_to_process(ctx) && !ctx.tasks.empty())
	{
		std::swap(ctx.tasks, ctx.processing_tasks);
        ctx.tasks.clear();
		ctx.processing_idx = 0;
	}

	return has_tasks_to_process(ctx);
}

void notify_for_exit(std::thread::id id)
{
	std::unique_lock<std::mutex> lock(global_data.mutex);
	auto it = global_data.contexts.find(id);
	if(it == global_data.contexts.end())
	{
		return;
	}

	auto ctx = it->second;
	std::lock_guard<std::mutex> remote_lock(ctx->tasks_mutex);
	lock.unlock();

	ctx->running = false;
	ctx->wakeup = true;
	ctx->wakeup_event.notify_all();
}

void register_thread(std::thread::id id)
{
	register_thread_impl(id);
}

std::thread::id get_main_id()
{
	return global_data.main_thread_id;
}

void invoke(std::thread::id id, task f)
{
	if(f == nullptr)
	{
		return;
	}
	std::unique_lock<std::mutex> lock(global_data.mutex);
	auto it = global_data.contexts.find(id);
	if(it == global_data.contexts.end())
	{
		return;
	}

	auto ctx = it->second;
	std::lock_guard<std::mutex> remote_lock(ctx->tasks_mutex);

	lock.unlock();

	ctx->tasks.emplace_back(std::move(f));

	ctx->wakeup = true;
	ctx->wakeup_event.notify_all();
}

void run_or_invoke(std::thread::id id, task f)
{
	if(!f)
	{
		return;
	}

	if(std::this_thread::get_id() == id)
	{
		f();
	}
	else
	{
		invoke(id, std::move(f));
	}
}

void notify(std::thread::id id)
{
	invoke(id, [](){});
}

namespace this_thread
{
namespace detail
{
bool process_one(std::unique_lock<std::mutex>& lock)
{
	if(local_data == nullptr)
	{
		return false;
	}
	auto& ctx = *local_data;

	if(prepare_tasks(ctx))
	{
		auto task = std::move(ctx.processing_tasks[ctx.processing_idx]);
		ctx.processing_idx++;

		lock.unlock();

		if(task)
		{
			task();
		}
		else
		{
			int a = 0;
			a++;
		}

		return true;
	}

	return false;
}

void process_all(std::unique_lock<std::mutex>& lock)
{
	if(local_data == nullptr)
	{
		return;
	}
	auto& ctx = *local_data;

	while(prepare_tasks(ctx))
	{
		auto task = std::move(ctx.processing_tasks[ctx.processing_idx]);
		ctx.processing_idx++;

		lock.unlock();

		if(task)
		{
			task();
		}

		lock.lock();
	}
}

void wait_for_event(const std::chrono::nanoseconds& wait_duration)
{
	if(local_data == nullptr)
	{
		return;
	}
	auto& ctx = *local_data;
	std::unique_lock<std::mutex> lock(ctx.tasks_mutex);

	if(true == process_one(lock))
	{
		return;
	}
	if(is_running() == false)
	{
		return;
	}

	// guard for spurious wakeups
	ctx.wakeup_event.wait_for(lock, wait_duration, [&ctx]() -> bool { return ctx.wakeup; });

	ctx.wakeup = false;

	process_one(lock);
}

void wait_for_event()
{
	if(local_data == nullptr)
	{
		return;
	}
	auto& ctx = *local_data;
	std::unique_lock<std::mutex> lock(ctx.tasks_mutex);

	if(true == process_one(lock))
	{
		return;
	}
	if(is_running() == false)
	{
		return;
	}

	// guard for spurious wakeups
	ctx.wakeup_event.wait(lock, [&ctx]() -> bool { return ctx.wakeup; });

	ctx.wakeup = false;

	process_one(lock);
}
}

void register_and_link()
{
	auto ctx = register_thread_impl(std::this_thread::get_id());
	set_local_data(ctx.get());
}

void unregister_and_unlink()
{
	unregister_thread_impl(std::this_thread::get_id());
	set_local_data(nullptr);
}

bool is_running()
{
	return local_data && local_data->running;
}

void process()
{
	if(local_data == nullptr)
	{
		return;
	}
	auto& ctx = *local_data;
	std::unique_lock<std::mutex> lock(ctx.tasks_mutex);

	detail::process_all(lock);
}

void wait_for_event()
{
	detail::wait_for_event();
}

bool is_main_thread()
{
	return std::this_thread::get_id() == global_data.main_thread_id;
}
}
}
