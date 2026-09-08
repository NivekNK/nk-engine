#include "nkpch.h"

#include "memory/synchronized_allocator.h"

namespace nk::mem {
    SynchronizedAllocator::SynchronizedAllocator(
        Untracked,
        Allocator& backing) noexcept {
        (void)_allocator_init_untracked<SynchronizedAllocator>(backing);
    }

    SynchronizedAllocator::~SynchronizedAllocator() {
        m_mutex.shutdown();
        m_backing = nullptr;
    }

    bool SynchronizedAllocator::init(Allocator& backing) noexcept {
        if (&backing == this || !backing.is_initialized() ||
            backing.is_tracked() ||
            backing.get_active_allocation_count() != 0) {
            return false;
        }
        auto initialized = m_mutex.init();
        if (!initialized)
            return false;
        m_backing = &backing;
        synchronize_statistics();
        return true;
    }

    void* SynchronizedAllocator::_allocate_raw(
        const u64 size_bytes,
        const u64 alignment) noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        return lock
            ? Allocator::_allocate_raw(size_bytes, alignment)
            : nullptr;
    }

    void* SynchronizedAllocator::_allocate_zeroed_raw(
        const u64 size_bytes,
        const u64 alignment) noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        return lock
            ? Allocator::_allocate_zeroed_raw(size_bytes, alignment)
            : nullptr;
    }

    bool SynchronizedAllocator::_free_raw(
        void* data,
        const u64 size_bytes) noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        return lock && Allocator::_free_raw(data, size_bytes);
    }

#if NK_MEMORY_TRACKING_ENABLED
    void* SynchronizedAllocator::_allocate_raw(
        cstr file,
        const u32 line,
        const u64 size_bytes,
        const u64 alignment) noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        return lock
            ? Allocator::_allocate_raw(file, line, size_bytes, alignment)
            : nullptr;
    }

    void* SynchronizedAllocator::_allocate_zeroed_raw(
        cstr file,
        const u32 line,
        const u64 size_bytes,
        const u64 alignment) noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        return lock
            ? Allocator::_allocate_zeroed_raw(
                  file,
                  line,
                  size_bytes,
                  alignment)
            : nullptr;
    }

    bool SynchronizedAllocator::_free_raw(
        cstr file,
        const u32 line,
        void* data,
        const u64 size_bytes) noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        return lock && Allocator::_free_raw(file, line, data, size_bytes);
    }
#endif

    void* SynchronizedAllocator::_do_allocate(
        const u64 size_bytes,
        const u64 alignment) noexcept {
        if (m_backing == nullptr)
            return nullptr;
        void* data = m_backing->_allocate_raw(size_bytes, alignment);
        synchronize_statistics();
        return data;
    }

    bool SynchronizedAllocator::_do_free(
        void* data,
        const u64 size_bytes) noexcept {
        if (m_backing == nullptr)
            return false;
        const bool released = m_backing->_free_raw(data, size_bytes);
        synchronize_statistics();
        return released;
    }

    void SynchronizedAllocator::synchronize_statistics() noexcept {
        if (m_backing == nullptr)
            return;
        const AllocatorStatistics current = m_backing->statistics();
        m_reserved_bytes = current.reserved_bytes;
        m_used_bytes = current.used_bytes;
        m_peak_used_bytes = current.peak_used_bytes;
        m_active_allocations = current.active_allocations;
        m_data = m_backing->get_data();
    }
}
