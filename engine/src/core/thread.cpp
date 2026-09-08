#include "nkpch.h"

#include "core/thread.h"

namespace nk {
    Thread::~Thread() {
        if (m_joinable)
            (void)join();
    }

    u32 Thread::invoke(void* thread) noexcept {
        Thread& self = *static_cast<Thread*>(thread);
        self.m_exit_code = self.m_entry(self.m_context);
        return self.m_exit_code;
    }

    LockGuard::~LockGuard() {
        if (m_mutex != nullptr)
            (void)m_mutex->unlock();
    }

    LockGuard::LockGuard(LockGuard&& other) noexcept
        : m_mutex{other.m_mutex} {
        other.m_mutex = nullptr;
    }

    result<LockGuard, thread_error> LockGuard::acquire(
        Mutex& mutex) noexcept {
        auto locked = mutex.lock();
        if (!locked)
            return err(locked.error());
        return ok(LockGuard{mutex});
    }

    Mutex::~Mutex() {
        shutdown();
    }

    ConditionVariable::~ConditionVariable() {
        shutdown();
    }
}
