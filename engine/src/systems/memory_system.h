#pragma once

#include "memory/allocation_tracker.h"

#if NK_MEMORY_TRACKING_ENABLED

    #include "core/format.h"
    #include "core/strview.h"
    #include "memory/malloc_allocator.h"
    #include "memory/memory_type.h"

namespace nk::mem {
    struct MemorySystemStorageAccess;

    enum class MemorySystemState : u8 {
        Cold,
        Bootstrapping,
        Ready,
        ShuttingDown,
        Stopped,
    };

    class MemorySystem final : public AllocationTracker {
    public:
        ~MemorySystem() override = default;

        static MemorySystem& init();
        static void shutdown();

        static MemorySystem& get() noexcept {
            static MemorySystem instance;
            return instance;
        }

        AllocatorId register_allocator(
            Allocator& allocator,
            const AllocatorDescriptor& descriptor) noexcept override;
        void unregister_allocator(AllocatorId allocator_id) noexcept override;
        void on_allocate(const AllocationEvent& event) noexcept override;
        FreeValidation validate_free(
            AllocatorId allocator_id,
            void* address,
            u64 size_bytes) noexcept override;
        void on_free(const AllocationEvent& event) noexcept override;
        void on_reset(const AllocatorResetEvent& event) noexcept override;
        cstr allocator_name(AllocatorId allocator_id) const noexcept override;

        static void log_report(bool detailed = false);
        static void log_report_intermediate();

        MemorySystemState state() const noexcept { return m_state; }
        u32 journal_count() const noexcept { return m_journal.count(); }
        u64 dropped_event_count() const noexcept {
            return m_dropped_event_count + m_journal.dropped_count();
        }
        u64 reentrant_event_count() const noexcept { return m_reentrant_event_count; }
        u64 metadata_failure_count() const noexcept { return m_metadata_failure_count; }
        u64 allocation_event_count() const noexcept { return m_allocation_event_count; }

    private:
        MemorySystem() noexcept = default;

        bool journal(const EarlyAllocationRecord& record) noexcept;
        void replay_journal();
        void apply_register(AllocatorId allocator_id, const AllocatorDescriptor& descriptor);
        void apply_unregister(AllocatorId allocator_id) noexcept;
        void apply_allocate(const AllocationEvent& event);
        void apply_free(const AllocationEvent& event);
        void apply_reset(const AllocatorResetEvent& event);

        void log_title(strview msg) {
            log("\033[38;2;170;129;246m", msg);
        }

        void log_info(strview msg) {
            log("\033[38;2;255;255;255m", msg);
        }

        void log_warn(strview msg) {
            log("\033[38;2;255;128;0m", msg);
        }

        void log_error(strview msg) {
            log("\033[38;2;233;38;109m", msg);
        }

        void log_trace(strview msg) {
            log("\033[38;2;218;218;218m", msg);
        }

        template <typename... Args>
        void log_info(format_string<std::type_identity_t<Args>...> pattern, Args&&... args) {
            log_formatted("\033[38;2;255;255;255m", pattern, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void log_warn(format_string<std::type_identity_t<Args>...> pattern, Args&&... args) {
            log_formatted("\033[38;2;255;128;0m", pattern, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void log_error(format_string<std::type_identity_t<Args>...> pattern, Args&&... args) {
            log_formatted("\033[38;2;233;38;109m", pattern, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void log_trace(format_string<std::type_identity_t<Args>...> pattern, Args&&... args) {
            log_formatted("\033[38;2;218;218;218m", pattern, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void log_formatted(
            cstr color,
            format_string<std::type_identity_t<Args>...> pattern,
            Args&&... args) {
            strbuf<1024> message;
            format_to(message, pattern, std::forward<Args>(args)...);
            message.mark_truncated();
            log(color, message.view());
        }

        void log(cstr color, strview msg);

        void* m_data = nullptr;
        MallocAllocator m_metadata_allocator{untracked};
        EarlyAllocationJournal m_journal{};
        AllocatorId m_next_allocator_id = native_allocator_id + 1;
        u64 m_dropped_event_count = 0;
        u64 m_reentrant_event_count = 0;
        u64 m_metadata_failure_count = 0;
        u64 m_allocation_event_count = 0;
        MemorySystemState m_state = MemorySystemState::Cold;

        friend struct MemorySystemStorageAccess;
    };
}

    #define NK_MEMORY_SYSTEM_INIT() \
        nk::mem::MemorySystem::init()
    #define NK_MEMORY_SYSTEM_SHUTDOWN() \
        nk::mem::MemorySystem::shutdown()
    #define NK_MEMORY_SYSTEM_LOG_REPORT() \
        nk::mem::MemorySystem::log_report()
    #define NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT() \
        nk::mem::MemorySystem::log_report(true)
    #define NK_MEMORY_SYSTEM_INTERMEDIATE_LOG_REPORT() \
        nk::mem::MemorySystem::log_report_intermediate()

#else

    #define NK_MEMORY_SYSTEM_INIT()
    #define NK_MEMORY_SYSTEM_SHUTDOWN()
    #define NK_MEMORY_SYSTEM_LOG_REPORT()
    #define NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT()
    #define NK_MEMORY_SYSTEM_INTERMEDIATE_LOG_REPORT()

#endif
