#pragma once
#include "detail/utility/apply.hpp"
#include "detail/utility/capture.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace itc
{
//-----------------------------------------------------------------------------
/// std::thread wrapper handling registration and exit notification
//-----------------------------------------------------------------------------
class thread : public std::thread
{
public:
	using id = std::uint64_t;

	thread() noexcept;
	thread(thread&&) noexcept = default;
	thread& operator=(thread&&) noexcept = default;

	template <typename F, typename... Args>
	explicit thread(F&& f, Args&&... args)
		: std::thread(std::forward<F>(f), std::forward<Args>(args)...)
	{
		register_this();
	}

	thread(const thread&) = delete;
	thread& operator=(const thread&) = delete;

	//-----------------------------------------------------------------------------
	/// Destructs the thread object. Notifies and joins if joinable.
	//-----------------------------------------------------------------------------
	~thread();

	//-----------------------------------------------------------------------------
	/// Returns the unique id of the thread
	//-----------------------------------------------------------------------------
	id get_id() const;

	//-----------------------------------------------------------------------------
	/// Notifies and waits for a thread to finish its execution
	//-----------------------------------------------------------------------------
	void join();

private:
	void register_this();

	/// associated thread id
	id id_ = 0;
};

//-----------------------------------------------------------------------------
/// Gets an invalid thread::id
//-----------------------------------------------------------------------------
constexpr inline thread::id invalid_id()
{
	return 0;
}

using shared_thread = std::shared_ptr<thread>;
using task = std::function<void()>;
using clock = std::chrono::steady_clock;

struct init_data
{
	std::function<void(const std::string&)> log_info;
	std::function<void(const std::string&)> log_error;
	std::function<void(std::thread&, const std::string&)> set_thread_name;
};

//-----------------------------------------------------------------------------
/// Inits the itc with user provided utility callbacks
//-----------------------------------------------------------------------------
void init(const init_data& data = {});

//-----------------------------------------------------------------------------
/// Shutdowns itc and waits for all registered threads to unregister themselves.
//-----------------------------------------------------------------------------
void shutdown(const std::chrono::seconds& wait_time = std::chrono::seconds(5));

//-----------------------------------------------------------------------------
/// Retrieves all registered thread ids.
//-----------------------------------------------------------------------------
std::vector<thread::id> get_all_registered_threads();

//-----------------------------------------------------------------------------
/// Retrieves the count of pending tasks for given thread id.
/// Useful for debug.
//-----------------------------------------------------------------------------
std::size_t get_pending_task_count(thread::id id);

//-----------------------------------------------------------------------------
/// Queues a task to be executed on the specified thread and notifies it.
//-----------------------------------------------------------------------------
template <typename F, typename... Args>
bool invoke(thread::id id, F&& f, Args&&... args);

//-----------------------------------------------------------------------------
/// If the calling thread is the same as the one passed it then
/// execute the task directly, else behave like invoke.
//-----------------------------------------------------------------------------
template <typename F, typename... Args>
bool run_or_invoke(thread::id id, F&& f, Args&&... args);

//-----------------------------------------------------------------------------
/// Wakes up a thread if sleeping via any of the itc blocking mechanisms.
//-----------------------------------------------------------------------------
void notify(thread::id id);

//-----------------------------------------------------------------------------
/// Wakes up and notifies a thread that it should start preparing for exit.
/// It does not join the thread.
//-----------------------------------------------------------------------------
void notify_for_exit(thread::id id);

//-----------------------------------------------------------------------------
/// Registers a thread by the native thread id.
/// Returns the unique generated id that can be used with the rest of the api.
//-----------------------------------------------------------------------------
thread::id register_thread(std::thread::id id);

//-----------------------------------------------------------------------------
/// Automatically register and run a thread with a prepared loop ready to be
/// invoked into.
//-----------------------------------------------------------------------------
thread make_thread(const std::string& name = "");

//-----------------------------------------------------------------------------
/// Automatically register and run a thread with a prepared loop ready to be
/// invoked into.
//-----------------------------------------------------------------------------
shared_thread make_shared_thread(const std::string& name = "");

namespace main_thread
{
//-----------------------------------------------------------------------------
/// Retrieves the main thread id.
//-----------------------------------------------------------------------------
thread::id get_id();
}

//-----------------------------------------------------------------------------
/// this_thread namespace describe actions you can do
/// while inside a thread's context
//-----------------------------------------------------------------------------
namespace this_thread
{
//-----------------------------------------------------------------------------
/// Registers this thread and links it for fast access.
//-----------------------------------------------------------------------------
void register_this_thread();

//-----------------------------------------------------------------------------
/// Unregisters this thread and unlinks it.
//-----------------------------------------------------------------------------
void unregister_this_thread();

//-----------------------------------------------------------------------------
/// Check whether this thread has been notified for exit.
//-----------------------------------------------------------------------------
bool notified_for_exit();

//-----------------------------------------------------------------------------
/// Gets the current thread id. Returns invalid id if not registered.
//-----------------------------------------------------------------------------
thread::id get_id();

//-----------------------------------------------------------------------------
/// Process all tasks.
//-----------------------------------------------------------------------------
void process();

//-----------------------------------------------------------------------------
/// Process all tasks until specified timeout_duration has elapsed.
//-----------------------------------------------------------------------------
template <typename Rep, typename Period>
void process_for(const std::chrono::duration<Rep, Period>& rtime);

//-----------------------------------------------------------------------------
/// Blocks until notified with an event and process it.
//-----------------------------------------------------------------------------
void wait();

//-----------------------------------------------------------------------------
/// Blocks until specified timeout_duration has elapsed or
/// notified, whichever comes first.
/// Returns value identifies the state of the result.
/// This function may block for longer than timeout_duration
/// due to scheduling or resource contention delays.
//-----------------------------------------------------------------------------
template <typename Rep, typename Period>
std::cv_status wait_for(const std::chrono::duration<Rep, Period>& rtime);

//-----------------------------------------------------------------------------
/// Blocks until specified abs_time has been reached or
/// notified, whichever comes first.
/// Returns value identifies the state of the result.
/// This function may block for longer than timeout_duration
/// due to scheduling or resource contention delays.
//-----------------------------------------------------------------------------
template <typename Clock, typename Duration>
std::cv_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time);

//-----------------------------------------------------------------------------
/// Sleeps for the specified duration and allow tasks to be processed during
/// that time.
//-----------------------------------------------------------------------------
template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period>& rtime);

//-----------------------------------------------------------------------------
/// Sleeps until the specified time has been reached and allow tasks to
/// be processed during that time.
//-----------------------------------------------------------------------------
template <typename Clock, typename Duration>
void sleep_until(const std::chrono::time_point<Clock, Duration>& abs_time);

//-----------------------------------------------------------------------------
/// Gets the current processing stack depth of this thread
//-----------------------------------------------------------------------------
std::uint32_t get_depth();
} // namespace this_thread
} // namespace itc

//-----------------------------------------------------------------------------
/// Impl
//-----------------------------------------------------------------------------
namespace itc
{
namespace detail
{

template <typename F, typename... Args>
task package_simple_task(F&& f, Args&&... args)
{
	return [callable = capture(std::forward<F>(f)), params = capture(std::forward<Args>(args)...)]() mutable
	{
		utility::apply(
			[&callable](std::decay_t<Args>&... args) {
				utility::invoke(std::forward<F>(std::get<0>(callable.get())), std::forward<Args>(args)...);
			},
			params.get());
	};
}

bool invoke_packaged_task(thread::id id, task& f);
} // namespace detail

// apply perfect forwarding to the callable and arguments
// so that so that using invoke/run_or_invoke will result
// in the same number of calls to constructors
template <typename F, typename... Args>
bool invoke(thread::id id, F&& f, Args&&... args)
{
	auto task = detail::package_simple_task(std::forward<F>(f), std::forward<Args>(args)...);
	return detail::invoke_packaged_task(id, task);
}

// apply perfect forwarding to the callable and arguments
// so that so that using invoke/run_or_invoke will result
// in the same number of calls to constructors
template <typename F, typename... Args>
bool run_or_invoke(thread::id id, F&& f, Args&&... args)
{
	if(this_thread::get_id() == id)
	{
		// directly call it
		utility::invoke(std::forward<F>(f), std::forward<Args>(args)...);
		return true;
	}
	else
	{
		return invoke(id, std::forward<F>(f), std::forward<Args>(args)...);
	}
}

namespace this_thread
{
namespace detail
{
std::cv_status wait_for(const std::chrono::microseconds& rtime);
void process_for(const std::chrono::microseconds& rtime);
} // namespace detail

template <typename Rep, typename Period>
void process_for(const std::chrono::duration<Rep, Period>& rtime)
{
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(rtime);
	detail::process_for(duration);
}

template <typename Rep, typename Period>
inline std::cv_status wait_for(const std::chrono::duration<Rep, Period>& rtime)
{
	if(rtime <= rtime.zero())
	{
		return std::cv_status::no_timeout;
	}

	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(rtime);

	return detail::wait_for(duration);
}

template <typename Clock, typename Duration>
inline std::cv_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time)
{
	return wait_for(abs_time.time_since_epoch() - Clock::now().time_since_epoch());
}

template <typename Rep, typename Period>
inline void sleep_for(const std::chrono::duration<Rep, Period>& rtime)
{
	if(rtime <= rtime.zero())
	{
		return;
	}

	auto now = clock::now();
	auto end_time = now + rtime;

	while(now < end_time)
	{
		if(notified_for_exit())
		{
			return;
		}
		auto time_left = end_time - now;

		wait_for(time_left);

		now = clock::now();
	}
}

template <typename Clock, typename Duration>
inline void sleep_until(const std::chrono::time_point<Clock, Duration>& abs_time)
{
	sleep_for(abs_time.time_since_epoch() - Clock::now().time_since_epoch());
}
} // namespace this_thread

} // namespace itc
