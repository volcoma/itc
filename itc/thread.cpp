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
	init_data utilities;
};

#define log_info_func(msg) log_info("[itc::" + std::string(__func__) + "] : " + (msg))
#define log_error_func(msg) log_error("[itc::" + std::string(__func__) + "] : " + (msg))

static shared_data global_state;
static thread_local thread_context* local_context_ptr = nullptr;
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
	if(global_state.utilities.set_thread_name && !name.empty())
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
	context->tasks.reserve(16);
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

void init(const init_data& data)
{
	this_thread::register_this_thread();

	std::unique_lock<std::mutex> lock(global_state.mutex);
	if(main_thread::get_id() != invalid_id())
	{
		log_error_func("Already initted.");
		return;
	}

	global_state.main_id = this_thread::get_id();
	global_state.utilities = data;

	log_info_func("Successful.");
}

void shutdown(const std::chrono::seconds& timeout)
{
	this_thread::unregister_this_thread();

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

size_t get_pending_task_count(thread::id id)
{
	if(id == invalid_id())
	{
		log_error_func("Invoking to an invalid thread.");
		return 0;
	}

	std::unique_lock<std::mutex> lock(global_state.mutex);

	auto it = global_state.contexts.find(id);
	if(it == global_state.contexts.end())
	{
		return 0;
	}

	auto context = it->second;
	std::lock_guard<std::mutex> remote_lock(context->tasks_mutex);

	lock.unlock();
	const auto left_to_process = context->processing_tasks.size() - context->processing_idx;
	const auto pending = context->tasks.size();
	return left_to_process + pending;
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

void notify(thread::id id)
{
	invoke(id, []() {});
}

namespace detail
{
// this function exists to avoid extra moves of the functor
// via the run_or_invoke
bool invoke_packaged_task(thread::id id, task& f)
{
	if(f == nullptr)
	{
		log_error_func("Invoking an invalid task.");
		return false;
	}
	if(id == invalid_id())
	{
		log_error_func("Invoking to an invalid thread.");
		return false;
	}

	std::unique_lock<std::mutex> lock(global_state.mutex);

	auto it = global_state.contexts.find(id);
	if(it == global_state.contexts.end())
	{
		return false;
	}

	auto context = it->second;
	std::lock_guard<std::mutex> remote_lock(context->tasks_mutex);

	lock.unlock();

	context->tasks.emplace_back(std::move(f));
	context->wakeup = true;
	context->wakeup_event.notify_all();
	return true;
}
} // namespace detail
namespace main_thread
{
thread::id get_id()
{
	return global_state.main_id;
}
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

void process_all_for(std::unique_lock<std::mutex>& lock, const std::chrono::microseconds& rtime)
{
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_and_link");
		return;
	}
	auto& local_context = get_local_context();

	auto now = clock::now();
	auto end_time = now + rtime;

	while(!notified_for_exit() && now < end_time && prepare_tasks(local_context))
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

		now = clock::now();
	}
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

	while(!notified_for_exit() && prepare_tasks(local_context))
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

void process_for(const std::chrono::microseconds& rtime)
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

	process_all_for(lock, rtime);
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

	process_all(lock);
}

std::cv_status wait_for(const std::chrono::microseconds& wait_duration)
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

void register_this_thread()
{
	auto context = register_thread_impl(std::this_thread::get_id());
	set_local_context(context.get());
}

void unregister_this_thread()
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
	detail::process();
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

} // namespace this_thread

thread make_thread(const std::string& name)
{
	thread t([]() {
		this_thread::register_this_thread();

		while(!this_thread::notified_for_exit())
		{
			this_thread::wait();
		}

		this_thread::unregister_this_thread();
	});

	name_thread(t, name);

	return t;
}

shared_thread make_shared_thread(const std::string& name)
{
	return std::make_shared<thread>(make_thread(name));
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

thread::thread() noexcept = default;

thread::~thread()
{
	if(joinable())
	{
		join();
	}
}

} // namespace itc
