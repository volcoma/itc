#include "thread.h"
#include "future.hpp"
#include "utility.hpp"
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
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

	std::atomic_bool exit = {false};
};

struct shared_data
{
	std::condition_variable cleanup_event;
	std::mutex mutex;
	std::map<std::thread::id, std::shared_ptr<context>> contexts;
	std::thread::id main_thread_id;
	utility_callbacks utilities;
};

static shared_data global_data;
static thread_local context* local_data_ptr = nullptr;

#define log_func(msg) log("[itc::" + std::string(__func__) + "] : " + msg)

void set_local_data(context* ctx)
{
	local_data_ptr = ctx;
}

bool has_local_data()
{
	return !!local_data_ptr;
}

context& get_local_data()
{
	return *local_data_ptr;
}

void name_thread(std::thread& th, const std::string& name)
{
	if(global_data.utilities.set_thread_name)
	{
		global_data.utilities.set_thread_name(th, name);
	}
}

void log(const std::string& name)
{
	if(global_data.utilities.logger)
	{
		global_data.utilities.logger(name);
	}
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

	// if this was the last entry then
	// notify that everything is cleaned up
	if(global_data.contexts.empty())
	{
		global_data.cleanup_event.notify_all();
	}
}

void init(const utility_callbacks& callbacks)
{
	std::unique_lock<std::mutex> lock(global_data.mutex);

	std::thread::id invalid;
	if(global_data.main_thread_id != invalid)
	{
		log_func("already initted");
		return;
	}

	global_data.main_thread_id = std::this_thread::get_id();
	global_data.utilities = callbacks;

	log_func("successful");
}

void shutdown()
{
	log_func("notifying and waiting for threads to complete");

	auto all_threads = get_all_registered_threads();
	for(const auto& id : all_threads)
	{
		notify_for_exit(id);
	}
	std::unique_lock<std::mutex> lock(global_data.mutex);
	global_data.cleanup_event.wait(lock, []() { return global_data.contexts.empty(); });

	log_func("successful");
}

std::vector<thread::id> get_all_registered_threads()
{
	std::vector<thread::id> result;
	std::unique_lock<std::mutex> lock(global_data.mutex);
	for(const auto& p : global_data.contexts)
	{
		result.emplace_back(p.first);
	}

	return result;
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

	auto remote_thread = it->second;
	std::lock_guard<std::mutex> remote_lock(remote_thread->tasks_mutex);

	lock.unlock();

	remote_thread->exit = true;
	remote_thread->wakeup = true;
	remote_thread->wakeup_event.notify_all();
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
		log_func("invalid task");
		return;
	}
	std::thread::id invalid;
	if(id == invalid)
	{
		log_func("invalid thread id");
		return;
	}

	std::unique_lock<std::mutex> lock(global_data.mutex);

	auto it = global_data.contexts.find(id);
	if(it == global_data.contexts.end())
	{
		return;
	}

	auto remote_thread = it->second;
	std::lock_guard<std::mutex> remote_lock(remote_thread->tasks_mutex);

	lock.unlock();

	remote_thread->tasks.emplace_back(std::move(f));
	remote_thread->wakeup = true;
	remote_thread->wakeup_event.notify_all();
}

void run_or_invoke(std::thread::id id, task func)
{
	if(std::this_thread::get_id() == id)
	{
		if(func)
		{
			func();
		}
	}
	else
	{
		invoke(id, std::move(func));
	}
}

void notify(std::thread::id id)
{
	invoke(id, []() {});
}

namespace this_thread
{
namespace detail
{
bool process_one(std::unique_lock<std::mutex>& lock)
{
	if(!has_local_data())
	{
		log_func("calling functions in the this_thread namespace "
				 "requires the thread to be already registered by calling "
				 "this_thread::register_and_link");

		return false;
	}
	auto& local_data = get_local_data();

	if(prepare_tasks(local_data))
	{
		auto task = std::move(local_data.processing_tasks[local_data.processing_idx]);
		local_data.processing_idx++;

		lock.unlock();

		if(task)
		{
			task();
		}

		return true;
	}

	return false;
}

void process_all(std::unique_lock<std::mutex>& lock)
{
	if(!has_local_data())
	{
		log_func("calling functions in the this_thread namespace "
				 "requires the thread to be already registered by calling "
				 "this_thread::register_and_link");
		return;
	}
	auto& local_data = get_local_data();

	while(prepare_tasks(local_data))
	{
		auto& processing_tasks = local_data.processing_tasks;
		auto& idx = local_data.processing_idx;
		auto task = std::move(processing_tasks[idx]);
		idx++;

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
	if(!has_local_data())
	{
		log_func("calling functions in the this_thread namespace "
				 "requires the thread to be already registered by calling "
				 "this_thread::register_and_link");
		return;
	}
	auto& local_data = get_local_data();

	std::unique_lock<std::mutex> lock(local_data.tasks_mutex);

	if(true == process_one(lock))
	{
		return;
	}
	if(notified_for_exit())
	{
		return;
	}

	// guard for spurious wakeups
	local_data.wakeup_event.wait_for(lock, wait_duration,
									 [&local_data]() -> bool { return local_data.wakeup; });

	local_data.wakeup = false;

	process_one(lock);
}

void wait_for_event()
{
	if(!has_local_data())
	{
		log_func("calling functions in the this_thread namespace "
				 "requires the thread to be already registered by calling "
				 "this_thread::register_and_link");
		return;
	}
	auto& local_data = get_local_data();

	std::unique_lock<std::mutex> lock(local_data.tasks_mutex);

	if(true == process_one(lock))
	{
		return;
	}
	if(notified_for_exit())
	{
		return;
	}

	// guard for spurious wakeups
	local_data.wakeup_event.wait(lock, [&local_data]() -> bool { return local_data.wakeup; });

	local_data.wakeup = false;

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

bool notified_for_exit()
{
	return has_local_data() && get_local_data().exit;
}

void process()
{
	if(!has_local_data())
	{
		log_func("calling functions in the this_thread namespace "
				 "requires the thread to be already registered by calling "
				 "this_thread::register_and_link");
		return;
	}
	auto& local_data = get_local_data();

	std::unique_lock<std::mutex> lock(local_data.tasks_mutex);

	detail::process_all(lock);
}

void wait_for_event()
{
	detail::wait_for_event();
}

bool is_main_thread()
{
	return std::this_thread::get_id() == get_main_id();
}
}

shared_thread run_thread(const std::string& name)
{
	auto th = std::make_shared<itc::thread>([]() {
		this_thread::register_and_link();

		while(!this_thread::notified_for_exit())
		{
			this_thread::wait_for_event();
		}

		this_thread::unregister_and_unlink();
	});

	name_thread(*th, name);

	return th;
}
}
