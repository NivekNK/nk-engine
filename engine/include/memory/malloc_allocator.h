#pragma once

#include "memory/allocator.h"

namespace nk {
    class MallocAllocator : public Allocator {
    public:
        MallocAllocator() : Allocator() {}

        virtual ~MallocAllocator() override = default;

        MallocAllocator(const MallocAllocator&) = delete;
        MallocAllocator& operator=(MallocAllocator&) = delete;

        MallocAllocator(MallocAllocator&& other)
            : Allocator{std::move(other)} {}

        MallocAllocator& operator=(MallocAllocator&& right) {
            Allocator::operator=(std::move(right));
            return *this;
        }

        NK_DEFINE_ALLOCATOR_INIT(MallocAllocator) {
            private_allocator_init(0, nullptr, name, memory_type);
        }

        virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) override;
        virtual void free_raw_impl(void* const ptr, const u64 size_bytes) override;
    };
}
