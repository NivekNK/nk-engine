#pragma once

#include "memory/allocator.h"
#include "memory/free_list.h"

namespace nk::mem {
    /**
     * Reuses allocations inside a fixed contiguous backing store.
     *
     * The backing store can be owned through another allocator or borrowed
     * from the caller. Free-range metadata always comes from an explicitly
     * injected allocator. Both allocators must outlive this object.
     */
    class FreeListAllocator final : public Allocator {
    public:
        static constexpr u64 allocation_overhead = sizeof(u64);

        FreeListAllocator() noexcept;
        FreeListAllocator(
            Untracked,
            Allocator& backing_allocator,
            Allocator& metadata_allocator,
            u64 size_bytes,
            u64 metadata_capacity) noexcept;
        FreeListAllocator(
            Untracked,
            Allocator& metadata_allocator,
            void* external_data,
            u64 size_bytes,
            u64 metadata_capacity) noexcept;
        ~FreeListAllocator() override;

        FreeListAllocator(FreeListAllocator&& other) noexcept;
        FreeListAllocator& operator=(FreeListAllocator&& other) noexcept;

        FreeListAllocator(const FreeListAllocator&) = delete;
        FreeListAllocator& operator=(const FreeListAllocator&) = delete;

        bool init(
            Allocator& backing_allocator,
            Allocator& metadata_allocator,
            u64 size_bytes,
            u64 metadata_capacity) noexcept;
        bool init(
            Allocator& metadata_allocator,
            void* external_data,
            u64 size_bytes,
            u64 metadata_capacity) noexcept;

        cstr to_cstr() const noexcept override {
            return "FreeListAllocator";
        }

        [[nodiscard]] bool owns_backing_memory() const noexcept {
            return m_owns_memory;
        }
        [[nodiscard]] Allocator* backing_allocator() const noexcept {
            return m_backing_allocator;
        }
        [[nodiscard]] Allocator* metadata_allocator() const noexcept {
            return m_metadata_allocator;
        }
        [[nodiscard]] u64 free_space() const noexcept {
            return m_free_list.free_space();
        }
        [[nodiscard]] u64 occupied_space() const noexcept {
            return m_free_list.used_space();
        }
        [[nodiscard]] u64 metadata_capacity() const noexcept {
            return m_free_list.metadata_capacity();
        }

    protected:
        void* _do_allocate(u64 size_bytes, u64 alignment) noexcept override;
        bool _do_free(void* data, u64 size_bytes) noexcept override;

    private:
        struct AllocationHeader {
            u64 guard;
        };

        static_assert(sizeof(AllocationHeader) == allocation_overhead);

        static constexpr u64 allocation_magic = 0x4e4b464c414c4f43ULL;

        bool valid_configuration(
            Allocator& metadata_allocator,
            u64 size_bytes,
            u64 metadata_capacity) const noexcept;
        static bool valid_address_range(
            void* data,
            u64 size_bytes) noexcept;
        bool release_resources() noexcept;
        static u64 header_guard(
            u64 base_address,
            u64 offset,
            u64 size_bytes) noexcept;
        void move_from(FreeListAllocator& other) noexcept;

        FreeList m_free_list;
        Allocator* m_backing_allocator;
        Allocator* m_metadata_allocator;
        bool m_owns_memory;
    };
}
