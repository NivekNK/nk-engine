#include "nkpch.h"

#include "memory/malloc_allocator.h"

namespace nk::mem {
    MallocAllocator::MallocAllocator()
        : Allocator() {}

    MallocAllocator::~MallocAllocator() {}

    MallocAllocator::MallocAllocator(MallocAllocator&& other)
        : Allocator(std::move(other)) {}

    MallocAllocator& MallocAllocator::operator=(MallocAllocator&& other) {
        Allocator::operator=(std::move(other));
        return *this;
    }

    void MallocAllocator::init() {
    }

    void* MallocAllocator::allocate_raw_impl(const u64 size_bytes, const u64 alignment) {
        this->allocation_count++;
        this->size_bytes += size_bytes;
        this->used_bytes += size_bytes;
        this->data = std::calloc(1, size_bytes);
        return this->data;
    }

    void MallocAllocator::free_raw_impl(void* const data, const u64 size_bytes) {
        this->allocation_count--;
        this->size_bytes -= size_bytes;
        this->used_bytes -= size_bytes;
        std::free(data);
        this->data = nullptr;
    }
}
