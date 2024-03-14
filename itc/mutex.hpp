#pragma once
#include "detail/semaphore.h"

namespace itc
{
class mutex
{
public:
    mutex() noexcept = default;

    mutex(mutex&& rhs) = delete;
    auto operator=(mutex&& rhs) -> mutex& = delete;

    mutex(const mutex&) = delete;
    auto operator=(const mutex&) -> mutex& = delete;

    //-----------------------------------------------------------------------------
    /// Locks the mutex. If another thread has already locked the mutex,
    /// a call to lock will block execution until the lock is acquired.
    /// If lock is called by a thread that already owns the mutex it will throw
    /// a std::system_error with error condition resource_deadlock_would_occur
    /// instead of deadlocking.
    //-----------------------------------------------------------------------------
    void lock()
    {
        auto id = this_thread::get_id();
        while(flag_.test_and_set())
        {
            // are we already owning it?
            if(owner_ == id)
            {
                throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));
            }
            sync_.wait();
        }

        owner_ = id;
    }

    //-----------------------------------------------------------------------------
    /// Tries to lock the mutex. Returns immediately.
    /// On successful lock acquisition returns true, otherwise returns false.
    //-----------------------------------------------------------------------------
    auto try_lock() -> bool
    {
        if(flag_.test_and_set())
        {
            return false;
        }

        owner_ = this_thread::get_id();
        return true;
    }

    //-----------------------------------------------------------------------------
    /// Unlocks the mutex.
    /// The mutex must be locked by the current thread of execution, otherwise,
    /// a std::system_error is thrown.
    //-----------------------------------------------------------------------------
    void unlock()
    {
        auto id = this_thread::get_id();

        // did we already release it.
        if(owner_ != id)
        {
            throw std::system_error(std::make_error_code(std::errc::no_lock_available));
        }

        owner_ = invalid_id();
        flag_.clear();
        sync_.notify_all();
    }

private:
    /// Underlying semaphore that handles notifying.
    detail::semaphore sync_;

    std::atomic<thread::id> owner_{invalid_id()};
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};
} // namespace itc
