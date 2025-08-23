#pragma once

#include "memory/allocator.h"

namespace nk::mem {
    class LinearAllocator : public Allocator {
    public:
        LinearAllocator();
        virtual ~LinearAllocator() override;

        LinearAllocator(LinearAllocator&& other);
        LinearAllocator& operator=(LinearAllocator&& other);

        LinearAllocator(LinearAllocator&) = delete;
        LinearAllocator& operator=(LinearAllocator&) = delete;

        void init(u64 size_bytes, void* data);

        virtual void* _allocate_raw(const u64 size_bytes, const u64 alignment) override;

        virtual bool _free_raw([[maybe_unused]] void* const data, [[maybe_unused]] const u64 size_bytes) override {
            ErrorLog("nk::mem::LinearAllocator does not support free, use free_linear_allocator.");
            return false;
        }

        bool _free_linear_allocator();

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        bool _free_linear_allocator(cstr file, u32 line);
#endif

        virtual cstr to_cstr() const override { return "LinearAllocator"; }

    private:
        bool m_owns_memory;
    };
}
