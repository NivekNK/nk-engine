#pragma once

#include "memory/allocator.h"

namespace nk::mem {
    class MallocAllocator : public Allocator {
    public:
        MallocAllocator();
        virtual ~MallocAllocator() override;

        MallocAllocator(MallocAllocator&& other);
        MallocAllocator& operator=(MallocAllocator&& other);

        MallocAllocator(MallocAllocator&) = delete;
        MallocAllocator& operator=(MallocAllocator&) = delete;

        void init();

        virtual void* _allocate_raw(const u64 size_bytes, const u64 alignment) override;
        virtual bool _free_raw(void* const data, const u64 size_bytes) override;

        virtual cstr to_cstr() const override { return "MallocAllocator"; }
    };
}
