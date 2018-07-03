#pragma once

#include <functional>
#include <chrono>
#include <thread>

namespace itc
{
using task = std::function<void()>;
using clock = std::chrono::high_resolution_clock;

template <typename Rep, typename Period>
void wait_for_cleanup(const std::chrono::duration<Rep, Period>& rtime);
void wait_for_cleanup();

//-----------------------------------------------------------------------------
//  Name : get_main_id ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
std::thread::id get_main_id();

//-----------------------------------------------------------------------------
//  Name : invoke ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void invoke(std::thread::id id, task f);

//-----------------------------------------------------------------------------
//  Name : run_or_invoke ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void run_or_invoke(std::thread::id id, task f);

//-----------------------------------------------------------------------------
//  Name : notify ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void notify(std::thread::id id);

//-----------------------------------------------------------------------------
//  Name : notify_for_exit ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void notify_for_exit(std::thread::id id);

//-----------------------------------------------------------------------------
//  Name : register_thread ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void register_thread(std::thread::id id);

//-----------------------------------------------------------------------------
//  Name : thread ()
/// <summary>
/// std::thread wrapper
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

namespace this_thread
{
//-----------------------------------------------------------------------------
//  Name : register_and_link ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void register_and_link();

//-----------------------------------------------------------------------------
//  Name : unregister_and_unlink ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void unregister_and_unlink();

//-----------------------------------------------------------------------------
//  Name : is_running ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
bool is_running();

//-----------------------------------------------------------------------------
//  Name : register_thread ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void process();

//-----------------------------------------------------------------------------
//  Name : is_main_thread ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
bool is_main_thread();

//-----------------------------------------------------------------------------
//  Name : wait_for_event ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
template <typename Rep, typename Period>
void wait_for_event(const std::chrono::duration<Rep, Period>& rtime);

//-----------------------------------------------------------------------------
//  Name : wait_for_event ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
void wait_for_event();

//-----------------------------------------------------------------------------
//  Name : sleep_for ()
/// <summary>
///
/// </summary>
//-----------------------------------------------------------------------------
template <typename Rep, typename Period>
void sleep_for(const std::chrono::duration<Rep, Period>& rtime);
}
}

//-----------------------------------------------------------------------------
/// Impl
//-----------------------------------------------------------------------------
namespace itc
{
namespace detail
{
void wait_for_cleanup(const std::chrono::nanoseconds& rtime);
}
template <typename Rep, typename Period>
inline void wait_for_cleanup(const std::chrono::duration<Rep, Period>& rtime)
{
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(rtime);
    detail::wait_for_cleanup(duration);
}


namespace this_thread
{
namespace detail
{
void wait_for_event(const std::chrono::nanoseconds& rtime);
}

template <typename Rep, typename Period>
inline void wait_for_event(const std::chrono::duration<Rep, Period>& rtime)
{
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(rtime);
    detail::wait_for_event(duration);
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
        if(!is_running())
        {
            return;
        }
        auto time_left = end_time - now;

        wait_for_event(time_left);

        now = clock::now();
    }
}
}
}
