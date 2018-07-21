#include "thread.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
namespace itc
{

struct thread_context
{
	std::atomic<thread::id> id = {invalid_id()};
	std::atomic<std::thread::id> native_thread_id;
	std::mutex tasks_mutex;
	std::vector<task> tasks;

	std::vector<task> processing_tasks;
	std::size_t processing_idx = 0;

	std::condition_variable wakeup_event;
	std::atomic<bool> wakeup = {false};

	std::atomic<bool> exit = {false};
};

struct shared_data
{
	std::atomic<thread::id> id_generator = {0};

	std::condition_variable cleanup_event;
	std::mutex mutex;
	std::unordered_map<std::thread::id, thread::id> id_map;
	std::unordered_map<thread::id, std::shared_ptr<thread_context>> contexts;
	thread::id main_id = invalid_id();
	utility_callbacks utilities;
};

static shared_data global_state;
static thread_local thread_context* local_context_ptr = nullptr;

#define log_info_func(msg) log_info("[itc::" + std::string(__func__) + "] : " + msg)
#define log_error_func(msg) log_error("[itc::" + std::string(__func__) + "] : " + msg)

void set_local_context(thread_context* context)
{
	local_context_ptr = context;
}

bool has_local_context()
{
	return !(local_context_ptr == nullptr);
}

thread_context& get_local_context()
{
	return *local_context_ptr;
}

void name_thread(std::thread& th, const std::string& name)
{
	if(global_state.utilities.set_thread_name)
	{
		global_state.utilities.set_thread_name(th, name);
	}
}

void log_info(const std::string& name)
{
	if(global_state.utilities.log_info)
	{
		global_state.utilities.log_info(name);
	}
}

void log_error(const std::string& name)
{
	if(global_state.utilities.log_error)
	{
		global_state.utilities.log_error(name);
	}
}

thread::id get_id(std::thread::id id)
{
	auto tidit = global_state.id_map.find(id);
	if(tidit != global_state.id_map.end())
	{
		return tidit->second;
	}
	return invalid_id();
}

std::shared_ptr<thread_context> register_thread_impl(std::thread::id native_thread_id)
{

	std::unique_lock<std::mutex> lock(global_state.mutex);
	auto id = get_id(native_thread_id);

	if(id == invalid_id())
	{
		id = ++global_state.id_generator;
	}

	auto it = global_state.contexts.find(id);
	if(it != global_state.contexts.end())
	{
		return it->second;
	}

	auto context = std::make_shared<thread_context>();

	context->native_thread_id = native_thread_id;
	context->id = id;
	global_state.id_map[native_thread_id] = id;
	global_state.contexts.emplace(id, context);
	return context;
}

void unregister_thread_impl(thread::id id)
{
	std::lock_guard<std::mutex> lock(global_state.mutex);
	auto it = global_state.contexts.find(id);
	if(it == global_state.contexts.end())
	{
		return;
	}

	// get the context and lock it
	auto context = it->second;
	std::lock_guard<std::mutex> local_lock(context->tasks_mutex);

	global_state.id_map.erase(context->native_thread_id);
	// now we can safely remove the context
	// from the global container and the local variable
	// will be the last reference to it
	global_state.contexts.erase(id);
	// if this was the last entry then
	// notify that everything is cleaned up
	if(global_state.contexts.empty())
	{
		global_state.cleanup_event.notify_all();
	}
}

void init(const utility_callbacks& callbacks)
{
	itc::this_thread::register_and_link();

	std::unique_lock<std::mutex> lock(global_state.mutex);
	if(global_state.main_id != invalid_id())
	{
		log_error_func("Already initted.");
		return;
	}

	global_state.main_id = this_thread::get_id();
	global_state.utilities = callbacks;

	log_info_func("Successful.");
}

void shutdown(const std::chrono::seconds& timeout)
{
	itc::this_thread::unregister_and_unlink();

	log_info_func("Notifying and waiting for threads to complete.");

	auto all_threads = get_all_registered_threads();
	for(const auto& id : all_threads)
	{
		notify_for_exit(id);
	}
	std::unique_lock<std::mutex> lock(global_state.mutex);

	auto result =
		global_state.cleanup_event.wait_for(lock, timeout, []() { return global_state.contexts.empty(); });

	if(result)
	{
		log_info_func("Successful.");
	}
	else
	{
		log_info_func("Timed out. Not all registered threads exited.");
	}
}

std::vector<thread::id> get_all_registered_threads()
{
	std::vector<thread::id> result;
	std::unique_lock<std::mutex> lock(global_state.mutex);
	for(const auto& p : global_state.contexts)
	{
		result.emplace_back(p.first);
	}

	return result;
}

bool has_tasks_to_process(const thread_context& context)
{
	return context.processing_idx < context.processing_tasks.size();
}

bool prepare_tasks(thread_context& context)
{
	if(!has_tasks_to_process(context) && !context.tasks.empty())
	{
		std::swap(context.tasks, context.processing_tasks);
		context.tasks.clear();
		context.processing_idx = 0;
	}

	return has_tasks_to_process(context);
}

void notify_for_exit(thread::id id)
{
	std::unique_lock<std::mutex> lock(global_state.mutex);

	auto it = global_state.contexts.find(id);
	if(it == global_state.contexts.end())
	{
		return;
	}

	auto context = it->second;
	std::lock_guard<std::mutex> remote_lock(context->tasks_mutex);

	lock.unlock();

	context->exit = true;
	context->wakeup = true;
	context->wakeup_event.notify_all();
}

thread::id register_thread(std::thread::id id)
{
	auto ctx = register_thread_impl(id);
	return ctx->id;
}

thread::id get_main_id()
{
	return global_state.main_id;
}

// this function exists to avoid extra moves of the functor
// via the run_or_invoke
void invoke_impl(thread::id id, task& f)
{
	if(f == nullptr)
	{
		log_error_func("Invoking an invalid task.");
		return;
	}
	if(id == invalid_id())
	{
		log_error_func("Invoking to an invalid thread.");
		return;
	}

	std::unique_lock<std::mutex> lock(global_state.mutex);

	auto it = global_state.contexts.find(id);
	if(it == global_state.contexts.end())
	{
		return;
	}

	auto context = it->second;
	std::lock_guard<std::mutex> remote_lock(context->tasks_mutex);

	lock.unlock();

	context->tasks.emplace_back(std::move(f));
	context->wakeup = true;
	context->wakeup_event.notify_all();
}

void invoke(thread::id id, task f)
{
	invoke_impl(id, f);
}

void run_or_invoke(thread::id id, task func)
{
	if(this_thread::get_id() == id)
	{
		if(func)
		{
			func();
		}
	}
	else
	{
		invoke_impl(id, func);
	}
}

void notify(thread::id id)
{
	invoke(id, []() {});
}

namespace this_thread
{
namespace detail
{
bool process_one(std::unique_lock<std::mutex>& lock)
{
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_and_link");

		return false;
	}
	auto& local_context = get_local_context();

	if(prepare_tasks(local_context))
	{
		auto task = std::move(local_context.processing_tasks[local_context.processing_idx]);
		local_context.processing_idx++;

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
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_and_link");
		return;
	}
	auto& local_context = get_local_context();

	while(prepare_tasks(local_context))
	{
		auto& processing_tasks = local_context.processing_tasks;
		auto& idx = local_context.processing_idx;
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

std::cv_status wait_for(const std::chrono::nanoseconds& wait_duration)
{
	auto status = std::cv_status::no_timeout;

	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_and_link");
		return status;
	}
	auto& local_context = get_local_context();

	std::unique_lock<std::mutex> lock(local_context.tasks_mutex);

	if(process_one(lock))
	{
		return status;
	}
	if(notified_for_exit())
	{
		return status;
	}

	local_context.wakeup = false;

	// guard for spurious wakeups
	while(!local_context.wakeup && status != std::cv_status::timeout)
	{
		status = local_context.wakeup_event.wait_for(lock, wait_duration);
	}

	local_context.wakeup = false;

	process_one(lock);

	return status;
}

void wait()
{
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_and_link");
		return;
	}
	auto& local_context = get_local_context();

	std::unique_lock<std::mutex> lock(local_context.tasks_mutex);

	if(process_one(lock))
	{
		return;
	}

	if(notified_for_exit())
	{
		return;
	}

	local_context.wakeup = false;

	// guard for spurious wakeups
	local_context.wakeup_event.wait(lock, [&local_context]() -> bool { return local_context.wakeup; });

	local_context.wakeup = false;

	process_one(lock);
}
} // namespace detail

void register_and_link()
{
	auto context = register_thread_impl(std::this_thread::get_id());
	set_local_context(context.get());
}

void unregister_and_unlink()
{
	unregister_thread_impl(get_id());
	set_local_context(nullptr);
}

bool notified_for_exit()
{
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_and_link");
		return true;
	}
	auto& local_context = get_local_context();

	return local_context.exit;
}

void process()
{
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_and_link");
		return;
	}
	auto& local_context = get_local_context();

	std::unique_lock<std::mutex> lock(local_context.tasks_mutex);

	detail::process_all(lock);
}

void wait()
{
	detail::wait();
}

thread::id get_id()
{
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_and_link");
		return invalid_id();
	}
	auto& local_context = get_local_context();
	return local_context.id;
}

bool is_main_thread()
{
	return this_thread::get_id() == get_main_id();
}
} // namespace this_thread

shared_thread run_thread(const std::string& name)
{
	auto th = std::make_shared<itc::thread>([]() {
		this_thread::register_and_link();

		while(!this_thread::notified_for_exit())
		{
			this_thread::wait();
		}

		this_thread::unregister_and_unlink();
	});

	name_thread(*th, name);

	return th;
}

thread::id invalid_id()
{
	return 0;
}

thread::id thread::get_id() const
{
	return id_;
}

void thread::join()
{
	notify_for_exit(get_id());
	std::thread::join();
}

void thread::register_this()
{
	id_ = register_thread(std::thread::get_id());
}

thread::~thread()
{
	if(joinable())
	{
		join();
	}
}
} // namespace itc
