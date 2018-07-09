#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <thread>
#include <vector>

namespace itc
{
using task = std::function<void()>;
using clock = std::chrono::steady_clock;

struct utility_callbacks
{
	std::function<void(const std::string&)> logger;
	std::function<void(std::thread&, const std::string&)> set_thread_name;
};

//-----------------------------------------------------------------------------
//  Name : init ()
/// <summary>
/// Inits the itc with user provided utility callbacks
/// </summary>
//-----------------------------------------------------------------------------
void init(const utility_callbacks& callbacks = {});

//-----------------------------------------------------------------------------
//  Name : shutdown ()
/// <summary>
/// Shutdowns itc and waits for all registered threads to unregister themselves.
/// </summary>
//-----------------------------------------------------------------------------
void shutdown();

//-----------------------------------------------------------------------------
//  Name : get_all_registered_threads ()
/// <summary>
/// Retrieves all registered thread ids.
/// </summary>
//-----------------------------------------------------------------------------
std::vector<std::thread::id> get_all_registered_threads();

//-----------------------------------------------------------------------------
//  Name : get_main_id ()
/// <summary>
/// Retrieves the main thread id.
/// </summary>
//-----------------------------------------------------------------------------
std::thread::id get_main_id();

//-----------------------------------------------------------------------------
//  Name : invoke ()
/// <summary>
/// Queues a task to be executed on the specified thread and notifies it.
/// </summary>
//-----------------------------------------------------------------------------
void invoke(std::thread::id id, task func);

//-----------------------------------------------------------------------------
//  Name : run_or_invoke ()
/// <summary>
/// If the thread is the current one then execute the task directly
/// else behave like invoke.
/// </summary>
//-----------------------------------------------------------------------------
void run_or_invoke(std::thread::id id, task func);

//-----------------------------------------------------------------------------
//  Name : notify ()
/// <summary>
/// Wakes up a thread if sleeping via any of the itc blocking mechanisms.
/// </summary>
//-----------------------------------------------------------------------------
void notify(std::thread::id id);

//-----------------------------------------------------------------------------
//  Name : notify_for_exit ()
/// <summary>
/// Wakes up and notifies a thread that it should start preparing for exit.
/// It does not join the thread.
/// </summary>
//-----------------------------------------------------------------------------
void notify_for_exit(std::thread::id id);

//-----------------------------------------------------------------------------
//  Name : register_thread ()
/// <summary>
/// registers a thread in the itc system
/// </summary>
//-----------------------------------------------------------------------------
void register_thread(std::thread::id id);

//-----------------------------------------------------------------------------
//  Name : thread ()
/// <summary>
/// std::thread wrapper handling registration and exit notification
/// </summary>
//-----------------------------------------------------------------------------
class thread : public std::thread
{
public:
	template <typename Callable, typename... Args>
	explicit thread(Callable&& f, Args&&... args)
		: std::thread(std::forward<Callable>(f), std::forward<Args>(args)...)
	{
		register_thread(get_id());
	}

	void join()
	{
		notify_for_exit(get_id());
		std::thread::join();
	}

	~thread()
	{
		notify_for_exit(get_id());
		if(joinable())
		{
			join();
		}
	}
};

using shared_thread = std::shared_ptr<thread>;
//-----------------------------------------------------------------------------
//  Name : run_thread ()
/// <summary>
/// Automatically register and run a thread with a prepared loop ready to be
/// invoked into.
/// </summary>
//-----------------------------------------------------------------------------
shared_thread run_thread(const std::string& name = "");

namespace this_thread
{
//-----------------------------------------------------------------------------
//  Name : register_and_link ()
/// <summary>
/// Registers this thread and links it for fast access.
/// </summary>
//-----------------------------------------------------------------------------
void register_and_link();

//-----------------------------------------------------------------------------
//  Name : unregister_and_unlink ()
/// <summary>
/// Unregisters this thread and unlinks it.
/// </summary>
//-----------------------------------------------------------------------------
void unregister_and_unlink();

//-----------------------------------------------------------------------------
//  Name : notified_for_exit ()
/// <summary>
/// Check whether this thread has been notified for exit.
/// </summary>
//-----------------------------------------------------------------------------
bool notified_for_exit();

//-----------------------------------------------------------------------------
//  Name : process ()
/// <summary>
/// Process all tasks.
/// </summary>
//-----------------------------------------------------------------------------
void process();

//-----------------------------------------------------------------------------
//  Name : is_main_thread ()
/// <summary>
/// Check is this thread the main thread
/// </summary>
//-----------------------------------------------------------------------------
bool is_main_thread();

//-----------------------------------------------------------------------------
//  Name : wait_for_event ()
/// <summary>
/// Blocks until specified timeout_duration has elapsed or
/// notified, whichever comes first.
/// Returns value identifies the state of the result.
/// This function may block for longer than timeout_duration
/// due to scheduling or resource contention delays.
/// </summary>
//-----------------------------------------------------------------------------
template <typename Rep, typename Period>
std::cv_status wait_for(const std::chrono::duration<Rep, Period>& rtime);

//-----------------------------------------------------------------------------
//  Name : wait_for_event ()
/// <summary>
/// Blocks until notified with an event and process it.
/// </summary>
//-----------------------------------------------------------------------------
void wait();

//-----------------------------------------------------------------------------
//  Name : sleep_for ()
/// <summary>
/// Sleeps for the specified duration and allow tasks to be processed during
/// that time.
/// </summary>
//-----------------------------------------------------------------------------
template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period>& rtime);

//-----------------------------------------------------------------------------
//  Name : sleep_until ()
/// <summary>
/// Sleeps until the specified time has been reached and allow tasks to
/// be processed during that time.
/// </summary>
//-----------------------------------------------------------------------------
template <typename Clock, typename Duration>
void sleep_until(const std::chrono::time_point<Clock, Duration>& abs_time);
}
}

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
		return std::cv_status::no_timeout;

	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(rtime);

	return detail::wait_for(duration);
}
template <typename Rep, typename Period>
inline void sleep_for(const std::chrono::duration<Rep, Period>& rtime)
{
	if(rtime <= rtime.zero())
		return;

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
}
}
