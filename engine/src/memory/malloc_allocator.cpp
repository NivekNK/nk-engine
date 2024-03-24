#include "nkpch.h"

#include "memory/malloc_allocator.h"

namespace nk {
    void* MallocAllocator::allocate_raw_impl(const u64 size_bytes, [[maybe_unused]] const u64 alignment) {
        m_allocation_count++;
        m_size_bytes += size_bytes;
        m_used_bytes += size_bytes;
        m_start = std::calloc(1, size_bytes);
        return m_start;
    }

    void MallocAllocator::free_raw_impl(void* const ptr, const u64 size_bytes) {
        m_allocation_count--;
        m_size_bytes -= size_bytes;
        m_used_bytes -= size_bytes;
        std::free(ptr);
        m_start = nullptr;
    }
}
