#pragma once
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

	template <typename F, typename... Args>
	explicit thread(F&& f, Args&&... args)
		: std::thread(std::forward<F>(f), std::forward<Args>(args)...)
	{
		register_this();
	}
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

	id id_;
};

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
/// Gets an invalid thread::id
//-----------------------------------------------------------------------------
thread::id invalid_id();

//-----------------------------------------------------------------------------
/// Retrieves the main thread id.
//-----------------------------------------------------------------------------
thread::id get_main_id();

//-----------------------------------------------------------------------------
/// Queues a task to be executed on the specified thread and notifies it.
//-----------------------------------------------------------------------------
void invoke(thread::id id, task func);

//-----------------------------------------------------------------------------
/// If the thread is the current one then execute the task directly
/// else behave like invoke.
//-----------------------------------------------------------------------------
void run_or_invoke(thread::id, task func);

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
shared_thread run_thread(const std::string& name = "");

namespace this_thread
{
//-----------------------------------------------------------------------------
/// Registers this thread and links it for fast access.
//-----------------------------------------------------------------------------
void register_and_link();

//-----------------------------------------------------------------------------
/// Unregisters this thread and unlinks it.
//-----------------------------------------------------------------------------
void unregister_and_unlink();

//-----------------------------------------------------------------------------
/// Check whether this thread has been notified for exit.
//-----------------------------------------------------------------------------
bool notified_for_exit();

//-----------------------------------------------------------------------------
/// Process all tasks.
//-----------------------------------------------------------------------------
void process();

//-----------------------------------------------------------------------------
/// Gets the current thread id. Returns invalid id if not registered.
//-----------------------------------------------------------------------------
thread::id get_id();

//-----------------------------------------------------------------------------
/// Check is this thread the main thread
//-----------------------------------------------------------------------------
bool is_main_thread();

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
} // namespace this_thread
} // namespace itc

//-----------------------------------------------------------------------------
/// Impl
//-----------------------------------------------------------------------------
namespace itc
{

namespace this_thread
{
namespace detail
{
std::cv_status wait_for(const std::chrono::nanoseconds& rtime);
}

template <typename Rep, typename Period>
inline std::cv_status wait_for(const std::chrono::duration<Rep, Period>& rtime)
{
	if(rtime <= rtime.zero())
	{
		return std::cv_status::no_timeout;
	}

	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(rtime);

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
