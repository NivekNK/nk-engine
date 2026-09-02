#pragma once

#include "memory/allocator.h"

namespace nk::mem {
    class LinearAllocator : public Allocator {
    public:
        LinearAllocator() noexcept;
        LinearAllocator(Untracked, u64 size_bytes, void* data = nullptr);
        ~LinearAllocator() override;

        LinearAllocator(LinearAllocator&& other) noexcept;
        LinearAllocator& operator=(LinearAllocator&& other) noexcept;

        LinearAllocator(LinearAllocator&) = delete;
        LinearAllocator& operator=(LinearAllocator&) = delete;

        void init(u64 size_bytes, void* data);

        bool _free_linear_allocator();

#if NK_MEMORY_TRACKING_ENABLED
        bool _free_linear_allocator(cstr file, u32 line);
#endif

        cstr to_cstr() const noexcept override { return "LinearAllocator"; }

    protected:
        void* _do_allocate(u64 size_bytes, u64 alignment) noexcept override;
        bool _do_free(void* data, u64 size_bytes) noexcept override;

    private:
        bool _reset(SourceLocation source) noexcept;
        bool m_owns_memory;
    };
}
