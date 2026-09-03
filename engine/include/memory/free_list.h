#pragma once

#include "collections/arr.h"
#include "core/result.h"

namespace nk::mem {
    struct MemoryRange {
        u64 offset = 0;
        u64 size = 0;
    };

    enum class free_list_error : u8 {
        not_initialized,
        already_initialized,
        invalid_size,
        invalid_alignment,
        invalid_range,
        capacity_overflow,
        out_of_memory,
        out_of_space,
        metadata_exhausted,
        overlapping_range,
        shrink_blocked,
    };

    /**
     * Tracks free subranges inside a logical contiguous address space.
     *
     * Metadata is allocated once from the allocator passed to init. Reserve,
     * release, clear and resize never allocate. The metadata allocator must
     * outlive this object.
     */
    class FreeList final {
    public:
        FreeList() noexcept = default;
        ~FreeList();

        FreeList(const FreeList&) = delete;
        FreeList& operator=(const FreeList&) = delete;
        FreeList(FreeList&&) = delete;
        FreeList& operator=(FreeList&&) = delete;

        [[nodiscard]] result<void, free_list_error> init(
            Allocator& metadata_allocator,
            u64 total_size,
            u64 metadata_capacity) noexcept;
        void shutdown() noexcept;

        [[nodiscard]] result<MemoryRange, free_list_error> reserve(
            u64 size,
            u64 alignment = 1) noexcept;
        [[nodiscard]] result<void, free_list_error> release(
            MemoryRange range) noexcept;
        [[nodiscard]] result<void, free_list_error> resize(
            u64 new_size) noexcept;
        [[nodiscard]] result<void, free_list_error> clear() noexcept;

        [[nodiscard]] bool initialized() const noexcept {
            return m_initialized;
        }
        [[nodiscard]] u64 total_size() const noexcept {
            return m_total_size;
        }
        [[nodiscard]] u64 free_space() const noexcept {
            return m_free_space;
        }
        [[nodiscard]] u64 used_space() const noexcept {
            return m_total_size - m_free_space;
        }
        [[nodiscard]] u64 range_count() const noexcept {
            return m_range_count;
        }
        [[nodiscard]] u64 metadata_capacity() const noexcept {
            return m_ranges.length();
        }
        [[nodiscard]] const MemoryRange* free_range(
            u64 index) const noexcept;

    private:
        static bool valid_alignment(u64 alignment) noexcept;
        static bool range_end(MemoryRange range, u64& end) noexcept;
        void insert_range(u64 index, MemoryRange range) noexcept;
        void remove_range(u64 index) noexcept;

        cl::arr<MemoryRange> m_ranges;
        u64 m_total_size = 0;
        u64 m_free_space = 0;
        u64 m_range_count = 0;
        bool m_initialized = false;
    };
}
