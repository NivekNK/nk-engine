#pragma once

#if defined(NK_DEV_MODE) && defined(NK_RELEASE_DEBUG_INFO) && \
    defined(NK_ACTIVE_MEMORY_SYSTEM) && \
    NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    #define NK_MEMORY_TRACKING_ENABLED 1
#else
    #define NK_MEMORY_TRACKING_ENABLED 0
#endif

namespace nk::mem {
    class Allocator;

    using AllocatorId = u32;

    inline constexpr AllocatorId native_allocator_id = 0;
    inline constexpr AllocatorId invalid_allocator_id = numeric::u32_max;

    struct Untracked {
        explicit constexpr Untracked() = default;
    };

    inline constexpr Untracked untracked{};

    struct SourceLocation {
        cstr file;
        u32 line;
    };

    struct AllocatorStatistics {
        u64 reserved_bytes;
        u64 used_bytes;
        u64 peak_used_bytes;
        u64 active_allocations;
    };

    struct AllocatorDescriptor {
        cstr name;
        cstr implementation;
        u32 memory_type;
        SourceLocation source;
        AllocatorStatistics statistics;
    };

    struct AllocationEvent {
        AllocatorId allocator_id;
        void* address;
        u64 size_bytes;
        u64 alignment;
        SourceLocation source;
        AllocatorStatistics statistics;
    };

    struct AllocatorResetEvent {
        AllocatorId allocator_id;
        SourceLocation source;
        AllocatorStatistics statistics;
    };

    enum class FreeValidation : u8 {
        Valid,
        UnknownAllocator,
        UnknownAddress,
        SizeMismatch,
        AlreadyFreed,
        TrackerUnavailable,
    };

    struct AllocationTracker {
        virtual ~AllocationTracker() = default;

        virtual AllocatorId register_allocator(
            Allocator& allocator,
            const AllocatorDescriptor& descriptor) noexcept = 0;
        virtual void unregister_allocator(AllocatorId allocator_id) noexcept = 0;
        virtual void on_allocate(const AllocationEvent& event) noexcept = 0;
        virtual FreeValidation validate_free(
            AllocatorId allocator_id,
            void* address,
            u64 size_bytes) noexcept = 0;
        virtual void on_free(const AllocationEvent& event) noexcept = 0;
        virtual void on_reset(const AllocatorResetEvent& event) noexcept = 0;
        virtual cstr allocator_name(AllocatorId allocator_id) const noexcept = 0;
    };

    enum class EarlyAllocationEventType : u8 {
        RegisterAllocator,
        UnregisterAllocator,
        Allocate,
        Free,
        Reset,
    };

    struct EarlyAllocationRecord {
        EarlyAllocationEventType type;
        AllocatorId allocator_id;
        AllocatorDescriptor descriptor;
        AllocationEvent allocation;
        AllocatorResetEvent reset;
    };

    class EarlyAllocationJournal {
    public:
        static constexpr u32 capacity = 1024;

        bool push(const EarlyAllocationRecord& record) noexcept {
            if (record.type == EarlyAllocationEventType::Free &&
                cancel_matching_allocation(record.allocation)) {
                return true;
            }

            if (m_count == capacity) {
                ++m_dropped_count;
                return false;
            }

            m_records[m_count++] = record;
            return true;
        }

        void clear() noexcept {
            m_count = 0;
            m_dropped_count = 0;
        }

        const EarlyAllocationRecord& operator[](u32 index) const noexcept {
            return m_records[index];
        }

        u32 count() const noexcept { return m_count; }
        u64 dropped_count() const noexcept { return m_dropped_count; }
        bool complete() const noexcept { return m_dropped_count == 0; }

    private:
        bool cancel_matching_allocation(const AllocationEvent& freed) noexcept {
            for (u32 index = m_count; index > 0; --index) {
                const EarlyAllocationRecord& candidate = m_records[index - 1];
                if ((candidate.type == EarlyAllocationEventType::Reset ||
                     candidate.type == EarlyAllocationEventType::UnregisterAllocator) &&
                    (candidate.type != EarlyAllocationEventType::Reset
                         ? candidate.allocator_id
                         : candidate.reset.allocator_id) == freed.allocator_id) {
                    return false;
                }
                if (candidate.type != EarlyAllocationEventType::Allocate ||
                    candidate.allocation.allocator_id != freed.allocator_id ||
                    candidate.allocation.address != freed.address) {
                    continue;
                }

                for (u32 move = index; move < m_count; ++move)
                    m_records[move - 1] = m_records[move];
                --m_count;
                return true;
            }
            return false;
        }

        EarlyAllocationRecord m_records[capacity]{};
        u32 m_count = 0;
        u64 m_dropped_count = 0;
    };

    static_assert(std::is_trivially_copyable_v<SourceLocation>);
    static_assert(std::is_trivially_copyable_v<AllocatorStatistics>);
    static_assert(std::is_trivially_copyable_v<AllocatorDescriptor>);
    static_assert(std::is_trivially_copyable_v<AllocationEvent>);
    static_assert(std::is_trivially_copyable_v<AllocatorResetEvent>);
    static_assert(std::is_trivially_copyable_v<EarlyAllocationRecord>);

#if NK_MEMORY_TRACKING_ENABLED
    // Transitional bridge used only by the legacy macros. New code should pass
    // an AllocationTracker& explicitly.
    AllocationTracker& default_allocation_tracker() noexcept;
#endif
}
