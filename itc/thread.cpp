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
	std::atomic<thread::id> id{invalid_id()};
	std::atomic<std::thread::id> native_thread_id;
	std::mutex tasks_mutex;
	std::vector<task> tasks;

	std::vector<task> processing_tasks;
	std::size_t processing_idx{0};
	std::size_t capacity_shrink_threashold{0};

	std::condition_variable wakeup_event;
	std::atomic<std::uint32_t> processing_stack_depth{0};

	std::atomic<bool> wakeup{false};
	std::atomic<bool> exit{false};
};

struct program_context
{
	std::atomic<thread::id> id_generator{};
	std::condition_variable cleanup_event;
	std::mutex mutex;
	std::unordered_map<std::thread::id, thread::id> id_map;
	std::unordered_map<thread::id, std::shared_ptr<thread_context>> contexts;
	thread::id main_thread_id{invalid_id()};
	std::atomic<size_t> init_count{0};
	init_data config;
};

#define log_info_func(msg) log_info("[itc::" + std::string(__func__) + "] : " + (msg))
#define log_error_func(msg) log_error("[itc::" + std::string(__func__) + "] : " + (msg))
namespace
{
program_context global_data;
thread_local thread_context* local_data = nullptr;
} // namespace

program_context& get_global_context()
{
	return global_data;
}
void set_local_context(thread_context* context)
{
	local_data = context;
}
bool has_local_context()
{
	return !(local_data == nullptr);
}
thread_context& get_local_context()
{
	return *local_data;
}

void name_thread(std::thread& th, const std::string& name)
{
	const auto& global_context = get_global_context();
	if(global_context.config.set_thread_name && !name.empty())
	{
		global_context.config.set_thread_name(th, name);
	}
}

void log_info(const std::string& name)
{
	const auto& global_context = get_global_context();
	if(global_context.config.log_info)
	{
		global_context.config.log_info(name);
	}
}

void log_error(const std::string& name)
{
	const auto& global_context = get_global_context();
	if(global_context.config.log_error)
	{
		global_context.config.log_error(name);
	}
}

std::shared_ptr<thread_context> register_thread_impl(std::thread::id native_thread_id)
{
	auto& global_context = get_global_context();
	std::unique_lock<std::mutex> lock(global_context.mutex);
	auto id = [&]() {
		auto tidit = global_context.id_map.find(native_thread_id);
		if(tidit != global_context.id_map.end())
		{
			return tidit->second;
		}
		return invalid_id();
	}();

	if(id == invalid_id())
	{
		id = ++global_context.id_generator;
	}

	auto it = global_context.contexts.find(id);
	if(it != global_context.contexts.end())
	{
		return it->second;
	}

	auto local_context = std::make_shared<thread_context>();
	local_context->tasks.reserve(16);
	local_context->native_thread_id = native_thread_id;
	local_context->id = id;
	local_context->capacity_shrink_threashold = global_context.config.capacity_shrink_threashold;
	global_context.id_map[native_thread_id] = id;
	global_context.contexts.emplace(id, local_context);
	return local_context;
}

void unregister_thread_impl(thread::id id)
{
	auto& global_context = get_global_context();
	std::lock_guard<std::mutex> lock(global_context.mutex);
	auto it = global_context.contexts.find(id);
	if(it == global_context.contexts.end())
	{
		return;
	}

	// get the context and lock it
	auto context = it->second;
	std::lock_guard<std::mutex> local_lock(context->tasks_mutex);

	global_context.id_map.erase(context->native_thread_id);
	// now we can safely remove the context
	// from the global container and the local variable
	// will be the last reference to it
	global_context.contexts.erase(id);
	// if this was the last entry then
	// notify that everything is cleaned up
	if(global_context.contexts.empty())
	{
		global_context.cleanup_event.notify_all();
	}
}

void init(const init_data& data)
{
	auto& global_context = get_global_context();
	if(global_context.init_count++ != 0)
	{
		return;
	}

	this_thread::register_this_thread();
	std::unique_lock<std::mutex> lock(global_context.mutex);
	global_context.main_thread_id = this_thread::get_id();
	global_context.config = data;
	log_info_func("Successful.");
}

int shutdown(const std::chrono::seconds& timeout)
{
	auto& global_context = get_global_context();
	if(global_context.init_count == 0)
	{
		log_error_func("Shutting down when not initted.");
		return -1;
	}

	if(--global_context.init_count != 0)
	{
		return -1;
	}

	this_thread::unregister_this_thread();
	log_info_func("Notifying and waiting for threads to complete.");
	auto all_threads = get_all_registered_threads();
	for(const auto& id : all_threads)
	{
		notify_for_exit(id);
	}
	std::unique_lock<std::mutex> lock(global_context.mutex);

	auto result = global_context.cleanup_event.wait_for(lock, timeout,
														[&]() { return global_context.contexts.empty(); });

	if(result)
	{
		log_info_func("Successful.");
		return 0;
	}
	else
	{
		log_info_func("Timed out. Not all registered threads exited.");
		return static_cast<int>(global_context.contexts.size());
	}
}

std::vector<thread::id> get_all_registered_threads()
{
	std::vector<thread::id> result;
	auto& global_context = get_global_context();
	std::unique_lock<std::mutex> lock(global_context.mutex);
	for(const auto& p : global_context.contexts)
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
	auto& global_context = get_global_context();
	std::unique_lock<std::mutex> lock(global_context.mutex);

	auto it = global_context.contexts.find(id);
	if(it == global_context.contexts.end())
	{
		return 0;
	}

	auto context = it->second;

    lock.unlock();

    std::lock_guard<std::mutex> remote_lock(context->tasks_mutex);

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
		if(context.tasks.capacity() > context.capacity_shrink_threashold)
		{
			context.tasks.shrink_to_fit();
		}
		context.processing_idx = 0;
	}

	return has_tasks_to_process(context);
}

void notify_for_exit(thread::id id)
{
	auto& global_context = get_global_context();
	std::unique_lock<std::mutex> lock(global_context.mutex);

	auto it = global_context.contexts.find(id);
	if(it == global_context.contexts.end())
	{
		return;
	}

	auto context = it->second;

    lock.unlock();

    std::lock_guard<std::mutex> remote_lock(context->tasks_mutex);

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
	auto& global_context = get_global_context();
	std::unique_lock<std::mutex> lock(global_context.mutex);

	auto it = global_context.contexts.find(id);
	if(it == global_context.contexts.end())
	{
		return false;
	}

	auto context = it->second;

    lock.unlock();

	std::lock_guard<std::mutex> remote_lock(context->tasks_mutex);

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
	const auto& global_context = get_global_context();
	return global_context.main_thread_id;
}
} // namespace main_thread
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
					   "this_thread::register_this_thread");

		return false;
	}
	auto& local_context = get_local_context();

	if(prepare_tasks(local_context))
	{
		auto task = std::move(local_context.processing_tasks[local_context.processing_idx]);
		local_context.processing_idx++;
		local_context.processing_stack_depth++;
		lock.unlock();

		if(task)
		{
			task();

            // invoke the tasks's destructor to allow
            // invoking from it on an unlocked mutex
            task = {};
		}

		lock.lock();
		local_context.processing_stack_depth--;

		return true;
	}

	return false;
}

void process_all_for(std::unique_lock<std::mutex>& lock, const std::chrono::microseconds& rtime)
{
	auto now = clock::now();
	auto end_time = now + rtime;

	while(!notified_for_exit() && now < end_time)
	{
		if(!process_one(lock))
		{
			break;
		}

		now = clock::now();
	}
}

void process_all(std::unique_lock<std::mutex>& lock)
{
	while(!notified_for_exit())
	{
		if(!process_one(lock))
		{
			break;
		}
	}
}

void process_for(const std::chrono::microseconds& rtime)
{
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_this_thread");
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
					   "this_thread::register_this_thread");
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
					   "this_thread::register_this_thread");
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
					   "this_thread::register_this_thread");
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
					   "this_thread::register_this_thread");
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
					   "this_thread::register_this_thread");
		return invalid_id();
	}
	auto& local_context = get_local_context();
	return local_context.id;
}

uint32_t get_depth()
{
	if(!has_local_context())
	{
		log_error_func("Calling functions in the this_thread namespace "
					   "requires the thread to be already registered by calling "
					   "this_thread::register_this_thread");
		return 0;
	}
	auto& local_context = get_local_context();
	return local_context.processing_stack_depth;
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
