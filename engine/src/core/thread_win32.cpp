#include "nkpch.h"

#include "core/thread.h"

#include <cerrno>
#include <process.h>

namespace nk {
    namespace {
        static_assert(sizeof(HANDLE) <= 16);
        static_assert(sizeof(SRWLOCK) <= 64);
        static_assert(sizeof(CONDITION_VARIABLE) <= 64);

        unsigned __stdcall thread_entry(void* thread) noexcept {
            return Thread::invoke(thread);
        }
    }

    result<void, thread_error> Thread::start(
        const ThreadEntry entry,
        void* context) noexcept {
        if (m_joinable || entry == nullptr)
            return err(thread_error{thread_error_code::invalid_state, 0});
        m_entry = entry;
        m_context = context;
        unsigned id = 0;
        const uintptr_t native = _beginthreadex(
            nullptr, 0, thread_entry, this, 0, &id);
        if (native == 0) {
            m_entry = nullptr;
            m_context = nullptr;
            return err(thread_error{
                thread_error_code::creation_failed,
                errno,
            });
        }
        const HANDLE handle = reinterpret_cast<HANDLE>(native);
        std::memcpy(m_native_handle, &handle, sizeof(handle));
        m_thread_id = id;
        m_joinable = true;
        return ok();
    }

    result<u32, thread_error> Thread::join() noexcept {
        if (!m_joinable)
            return err(thread_error{thread_error_code::invalid_state, 0});
        HANDLE handle = nullptr;
        std::memcpy(&handle, m_native_handle, sizeof(handle));
        const DWORD waited = WaitForSingleObject(handle, INFINITE);
        if (waited != WAIT_OBJECT_0) {
            return err(thread_error{
                thread_error_code::join_failed,
                static_cast<i32>(GetLastError()),
            });
        }
        DWORD exit_code = 0;
        if (!GetExitCodeThread(handle, &exit_code)) {
            return err(thread_error{
                thread_error_code::join_failed,
                static_cast<i32>(GetLastError()),
            });
        }
        CloseHandle(handle);
        m_joinable = false;
        m_thread_id = 0;
        m_entry = nullptr;
        m_context = nullptr;
        std::memset(m_native_handle, 0, sizeof(m_native_handle));
        return ok(static_cast<u32>(exit_code));
    }

    u64 Thread::current_id() noexcept {
        return static_cast<u64>(GetCurrentThreadId());
    }

    u32 Thread::logical_processor_count() noexcept {
        const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        return count == 0 ? 1u : static_cast<u32>(count);
    }

    result<void, thread_error> Mutex::init() noexcept {
        if (m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        InitializeSRWLock(reinterpret_cast<SRWLOCK*>(m_native));
        m_initialized = true;
        return ok();
    }

    result<void, thread_error> Mutex::lock() noexcept {
        if (!m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        AcquireSRWLockExclusive(reinterpret_cast<SRWLOCK*>(m_native));
        return ok();
    }

    result<void, thread_error> Mutex::unlock() noexcept {
        if (!m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        ReleaseSRWLockExclusive(reinterpret_cast<SRWLOCK*>(m_native));
        return ok();
    }

    void Mutex::shutdown() noexcept {
        if (!m_initialized)
            return;
        std::memset(m_native, 0, sizeof(m_native));
        m_initialized = false;
    }

    result<void, thread_error> ConditionVariable::init() noexcept {
        if (m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        InitializeConditionVariable(
            reinterpret_cast<CONDITION_VARIABLE*>(m_native));
        m_initialized = true;
        return ok();
    }

    result<void, thread_error> ConditionVariable::wait(
        LockGuard& lock) noexcept {
        if (!m_initialized || lock.m_mutex == nullptr ||
            !lock.m_mutex->m_initialized) {
            return err(thread_error{thread_error_code::invalid_state, 0});
        }
        if (!SleepConditionVariableSRW(
                reinterpret_cast<CONDITION_VARIABLE*>(m_native),
                reinterpret_cast<SRWLOCK*>(lock.m_mutex->m_native),
                INFINITE,
                0)) {
            return err(thread_error{
                thread_error_code::condition_failed,
                static_cast<i32>(GetLastError()),
            });
        }
        return ok();
    }

    result<void, thread_error> ConditionVariable::notify_one() noexcept {
        if (!m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        WakeConditionVariable(
            reinterpret_cast<CONDITION_VARIABLE*>(m_native));
        return ok();
    }

    result<void, thread_error> ConditionVariable::notify_all() noexcept {
        if (!m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        WakeAllConditionVariable(
            reinterpret_cast<CONDITION_VARIABLE*>(m_native));
        return ok();
    }

    void ConditionVariable::shutdown() noexcept {
        if (!m_initialized)
            return;
        std::memset(m_native, 0, sizeof(m_native));
        m_initialized = false;
    }
}
