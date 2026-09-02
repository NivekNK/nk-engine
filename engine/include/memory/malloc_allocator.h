#pragma once

#include "memory/allocator.h"

namespace nk::mem {
    class MallocAllocator : public Allocator {
    public:
        MallocAllocator() noexcept;
        explicit MallocAllocator(Untracked);
        ~MallocAllocator() override;

        MallocAllocator(MallocAllocator&& other) noexcept;
        MallocAllocator& operator=(MallocAllocator&& other) noexcept;

        MallocAllocator(MallocAllocator&) = delete;
        MallocAllocator& operator=(MallocAllocator&) = delete;

        void init();

        cstr to_cstr() const noexcept override { return "MallocAllocator"; }

    protected:
        void* _do_allocate(u64 size_bytes, u64 alignment) noexcept override;
        bool _do_free(void* data, u64 size_bytes) noexcept override;
    };
}
