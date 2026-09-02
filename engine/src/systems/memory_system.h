#pragma once

#include "memory/allocation_tracker.h"

#if NK_MEMORY_TRACKING_ENABLED

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

    private:
        MemorySystem() noexcept = default;

        bool journal(const EarlyAllocationRecord& record) noexcept;
        void replay_journal();
        void apply_register(AllocatorId allocator_id, const AllocatorDescriptor& descriptor);
        void apply_unregister(AllocatorId allocator_id) noexcept;
        void apply_allocate(const AllocationEvent& event);
        void apply_free(const AllocationEvent& event);
        void apply_reset(const AllocatorResetEvent& event);

        void log_title(std::string_view msg) {
            log("\033[38;2;170;129;246m", msg.data(), msg.length());
        }

        void log_info(std::string_view msg) {
            log("\033[38;2;255;255;255m", msg.data(), msg.length());
        }

        void log_warn(std::string_view msg) {
            log("\033[38;2;255;128;0m", msg.data(), msg.length());
        }

        void log_error(std::string_view msg) {
            log("\033[38;2;233;38;109m", msg.data(), msg.length());
        }

        void log_trace(std::string_view msg) {
            log("\033[38;2;218;218;218m", msg.data(), msg.length());
        }

        void log(cstr color, cstr msg, std::size_t msg_size);

        void* m_data = nullptr;
        EarlyAllocationJournal m_journal{};
        AllocatorId m_next_allocator_id = native_allocator_id + 1;
        u64 m_dropped_event_count = 0;
        u64 m_reentrant_event_count = 0;
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
