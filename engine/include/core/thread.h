#pragma once

#include "core/defines.h"
#include "core/result.h"

namespace nk {
    enum class thread_error_code : u8 {
        invalid_state,
        creation_failed,
        join_failed,
        mutex_failed,
        condition_failed,
    };

    struct thread_error {
        thread_error_code code;
        i32 native_code = 0;
    };

    using ThreadEntry = u32 (*)(void*) noexcept;

    class Thread final {
    public:
        Thread() noexcept = default;
        ~Thread();

        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;
        Thread(Thread&&) = delete;
        Thread& operator=(Thread&&) = delete;

        [[nodiscard]] result<void, thread_error> start(
            ThreadEntry entry,
            void* context) noexcept;
        [[nodiscard]] result<u32, thread_error> join() noexcept;

        [[nodiscard]] bool joinable() const noexcept { return m_joinable; }
        [[nodiscard]] u64 id() const noexcept { return m_thread_id; }
        [[nodiscard]] static u64 current_id() noexcept;
        [[nodiscard]] static u32 logical_processor_count() noexcept;

        // Platform trampoline entry; callers should use start().
        static u32 invoke(void* thread) noexcept;

    private:
        alignas(16) u8 m_native_handle[16]{};
        ThreadEntry m_entry = nullptr;
        void* m_context = nullptr;
        u64 m_thread_id = 0;
        u32 m_exit_code = 0;
        bool m_joinable = false;
    };

    class Mutex final {
    public:
        Mutex() noexcept = default;
        ~Mutex();

        Mutex(const Mutex&) = delete;
        Mutex& operator=(const Mutex&) = delete;
        Mutex(Mutex&&) = delete;
        Mutex& operator=(Mutex&&) = delete;

        [[nodiscard]] result<void, thread_error> init() noexcept;
        [[nodiscard]] result<void, thread_error> lock() noexcept;
        [[nodiscard]] result<void, thread_error> unlock() noexcept;
        void shutdown() noexcept;
        [[nodiscard]] bool initialized() const noexcept { return m_initialized; }

    private:
        alignas(16) u8 m_native[64]{};
        bool m_initialized = false;

        friend class ConditionVariable;
    };

    class LockGuard final {
    public:
        ~LockGuard();

        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;
        LockGuard(LockGuard&& other) noexcept;
        LockGuard& operator=(LockGuard&&) = delete;

        [[nodiscard]] static result<LockGuard, thread_error> acquire(
            Mutex& mutex) noexcept;
        [[nodiscard]] bool owns_lock() const noexcept {
            return m_mutex != nullptr;
        }

    private:
        explicit LockGuard(Mutex& mutex) noexcept : m_mutex{&mutex} {}
        Mutex* m_mutex = nullptr;

        friend class ConditionVariable;
    };

    class ConditionVariable final {
    public:
        ConditionVariable() noexcept = default;
        ~ConditionVariable();

        ConditionVariable(const ConditionVariable&) = delete;
        ConditionVariable& operator=(const ConditionVariable&) = delete;
        ConditionVariable(ConditionVariable&&) = delete;
        ConditionVariable& operator=(ConditionVariable&&) = delete;

        [[nodiscard]] result<void, thread_error> init() noexcept;
        [[nodiscard]] result<void, thread_error> wait(
            LockGuard& lock) noexcept;
        [[nodiscard]] result<void, thread_error> notify_one() noexcept;
        [[nodiscard]] result<void, thread_error> notify_all() noexcept;
        void shutdown() noexcept;
        [[nodiscard]] bool initialized() const noexcept { return m_initialized; }

    private:
        alignas(16) u8 m_native[64]{};
        bool m_initialized = false;
    };
}
