#pragma once

#include "core/thread.h"
#include "memory/allocator.h"

namespace nk::mem {
    // Serializes a single, otherwise thread-confined allocator. The backing
    // allocator must be initialized, untracked and outlive this wrapper. This
    // keeps synchronization opt-in and ensures every allocation is reported
    // exactly once through the wrapper's tracker and original source location.
    class SynchronizedAllocator final : public Allocator {
    public:
        SynchronizedAllocator() noexcept = default;
        SynchronizedAllocator(Untracked, Allocator& backing) noexcept;
        ~SynchronizedAllocator() override;

        SynchronizedAllocator(const SynchronizedAllocator&) = delete;
        SynchronizedAllocator& operator=(const SynchronizedAllocator&) = delete;
        SynchronizedAllocator(SynchronizedAllocator&&) = delete;
        SynchronizedAllocator& operator=(SynchronizedAllocator&&) = delete;

        [[nodiscard]] bool init(Allocator& backing) noexcept;

        void* _allocate_raw(u64 size_bytes, u64 alignment) noexcept override;
        void* _allocate_zeroed_raw(
            u64 size_bytes,
            u64 alignment) noexcept override;
        bool _free_raw(void* data, u64 size_bytes) noexcept override;

#if NK_MEMORY_TRACKING_ENABLED
        void* _allocate_raw(
            cstr file,
            u32 line,
            u64 size_bytes,
            u64 alignment) noexcept override;
        void* _allocate_zeroed_raw(
            cstr file,
            u32 line,
            u64 size_bytes,
            u64 alignment) noexcept override;
        bool _free_raw(
            cstr file,
            u32 line,
            void* data,
            u64 size_bytes) noexcept override;
#endif

        [[nodiscard]] Allocator* backing_allocator() const noexcept {
            return m_backing;
        }

        cstr to_cstr() const noexcept override {
            return "SynchronizedAllocator";
        }

    protected:
        void* _do_allocate(u64 size_bytes, u64 alignment) noexcept override;
        bool _do_free(void* data, u64 size_bytes) noexcept override;

    private:
        void synchronize_statistics() noexcept;

        Allocator* m_backing = nullptr;
        Mutex m_mutex;
    };
}
