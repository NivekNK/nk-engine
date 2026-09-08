#include "nkpch.h"

#include "core/thread.h"

#include <cerrno>
#include <pthread.h>

namespace nk {
    namespace {
        static_assert(sizeof(pthread_t) <= 16);
        static_assert(sizeof(pthread_t) <= sizeof(u64));
        static_assert(alignof(pthread_t) <= 16);
        static_assert(sizeof(pthread_mutex_t) <= 64);
        static_assert(alignof(pthread_mutex_t) <= 16);
        static_assert(sizeof(pthread_cond_t) <= 64);
        static_assert(alignof(pthread_cond_t) <= 16);

    }

    result<void, thread_error> Thread::start(
        const ThreadEntry entry,
        void* context) noexcept {
        if (m_joinable || entry == nullptr)
            return err(thread_error{thread_error_code::invalid_state, 0});
        m_entry = entry;
        m_context = context;
        pthread_t handle{};
        auto trampoline = +[](void* thread) noexcept -> void* {
            (void)Thread::invoke(thread);
            return nullptr;
        };
        const i32 native = pthread_create(
            &handle,
            nullptr,
            trampoline,
            this);
        if (native != 0) {
            m_entry = nullptr;
            m_context = nullptr;
            return err(thread_error{
                thread_error_code::creation_failed,
                native,
            });
        }
        std::memcpy(m_native_handle, &handle, sizeof(handle));
        std::memcpy(&m_thread_id, &handle, sizeof(handle));
        m_joinable = true;
        return ok();
    }

    result<u32, thread_error> Thread::join() noexcept {
        if (!m_joinable)
            return err(thread_error{thread_error_code::invalid_state, 0});
        pthread_t handle{};
        std::memcpy(&handle, m_native_handle, sizeof(handle));
        void* value = nullptr;
        const i32 native = pthread_join(handle, &value);
        if (native != 0)
            return err(thread_error{thread_error_code::join_failed, native});
        m_joinable = false;
        m_thread_id = 0;
        m_entry = nullptr;
        m_context = nullptr;
        std::memset(m_native_handle, 0, sizeof(m_native_handle));
        return ok(m_exit_code);
    }

    u64 Thread::current_id() noexcept {
        return static_cast<u64>(os::get_thread_id());
    }

    u32 Thread::logical_processor_count() noexcept {
        const long count = sysconf(_SC_NPROCESSORS_ONLN);
        return count > 0 && static_cast<unsigned long>(count) <= numeric::u32_max
            ? static_cast<u32>(count)
            : 1u;
    }

    result<void, thread_error> Mutex::init() noexcept {
        if (m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        const i32 native = pthread_mutex_init(
            reinterpret_cast<pthread_mutex_t*>(m_native),
            nullptr);
        if (native != 0)
            return err(thread_error{thread_error_code::mutex_failed, native});
        m_initialized = true;
        return ok();
    }

    result<void, thread_error> Mutex::lock() noexcept {
        if (!m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        const i32 native = pthread_mutex_lock(
            reinterpret_cast<pthread_mutex_t*>(m_native));
        return native == 0
            ? result<void, thread_error>{ok()}
            : result<void, thread_error>{err(thread_error{
                thread_error_code::mutex_failed,
                native,
            })};
    }

    result<void, thread_error> Mutex::unlock() noexcept {
        if (!m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        const i32 native = pthread_mutex_unlock(
            reinterpret_cast<pthread_mutex_t*>(m_native));
        return native == 0
            ? result<void, thread_error>{ok()}
            : result<void, thread_error>{err(thread_error{
                thread_error_code::mutex_failed,
                native,
            })};
    }

    void Mutex::shutdown() noexcept {
        if (!m_initialized)
            return;
        if (pthread_mutex_destroy(
                reinterpret_cast<pthread_mutex_t*>(m_native)) != 0) {
            return;
        }
        std::memset(m_native, 0, sizeof(m_native));
        m_initialized = false;
    }

    result<void, thread_error> ConditionVariable::init() noexcept {
        if (m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        const i32 native = pthread_cond_init(
            reinterpret_cast<pthread_cond_t*>(m_native),
            nullptr);
        if (native != 0) {
            return err(thread_error{
                thread_error_code::condition_failed,
                native,
            });
        }
        m_initialized = true;
        return ok();
    }

    result<void, thread_error> ConditionVariable::wait(
        LockGuard& lock) noexcept {
        if (!m_initialized || lock.m_mutex == nullptr ||
            !lock.m_mutex->m_initialized) {
            return err(thread_error{thread_error_code::invalid_state, 0});
        }
        const i32 native = pthread_cond_wait(
            reinterpret_cast<pthread_cond_t*>(m_native),
            reinterpret_cast<pthread_mutex_t*>(lock.m_mutex->m_native));
        return native == 0
            ? result<void, thread_error>{ok()}
            : result<void, thread_error>{err(thread_error{
                thread_error_code::condition_failed,
                native,
            })};
    }

    result<void, thread_error> ConditionVariable::notify_one() noexcept {
        if (!m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        const i32 native = pthread_cond_signal(
            reinterpret_cast<pthread_cond_t*>(m_native));
        return native == 0
            ? result<void, thread_error>{ok()}
            : result<void, thread_error>{err(thread_error{
                thread_error_code::condition_failed,
                native,
            })};
    }

    result<void, thread_error> ConditionVariable::notify_all() noexcept {
        if (!m_initialized)
            return err(thread_error{thread_error_code::invalid_state, 0});
        const i32 native = pthread_cond_broadcast(
            reinterpret_cast<pthread_cond_t*>(m_native));
        return native == 0
            ? result<void, thread_error>{ok()}
            : result<void, thread_error>{err(thread_error{
                thread_error_code::condition_failed,
                native,
            })};
    }

    void ConditionVariable::shutdown() noexcept {
        if (!m_initialized)
            return;
        if (pthread_cond_destroy(
                reinterpret_cast<pthread_cond_t*>(m_native)) != 0) {
            return;
        }
        std::memset(m_native, 0, sizeof(m_native));
        m_initialized = false;
    }
}
