#include "nkpch.h"

#include "memory/malloc_allocator.h"

namespace nk {
    MallocAllocator::MallocAllocator()
        : Allocator() {}

    MallocAllocator::~MallocAllocator() {}

    MallocAllocator::MallocAllocator(MallocAllocator&& other)
        : Allocator(std::move(other)) {}

    MallocAllocator& MallocAllocator::operator=(MallocAllocator&& other) {
        Allocator::operator=(std::move(other));
        return *this;
    }

    Allocator* MallocAllocator::init() {
        return this;
    }

    void* MallocAllocator::allocate_raw_impl(const u64 size_bytes, const u64 alignment) {
        m_allocation_count++;
        m_size_bytes += size_bytes;
        m_used_bytes += size_bytes;
        m_data = std::calloc(1, size_bytes);
        return m_data;
    }

    void MallocAllocator::free_raw_impl(void* const data, const u64 size_bytes) {
        m_allocation_count--;
        m_size_bytes -= size_bytes;
        m_used_bytes -= size_bytes;
        std::free(data);
        m_data = nullptr;
    }
}
