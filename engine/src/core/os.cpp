#include "nkpch.h"

#include "core/os.h"

#include "systems/memory_system.h"

namespace nk::os {
    void* native_allocate_impl(u64 size_bytes, u64 alignment) {
        return malloc(size_bytes);
    }

    void native_free_impl(void* data, u64 size_bytes) {
        free(data);
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    void* _native_allocate_impl(cstr file, u32 line, u64 size_bytes, u64 alignment) {
        nk::mem::MemorySystem::native_allocation(file, line, size_bytes, mem::AllocationType::Allocate);
        return malloc(size_bytes);
    }

    void _native_free_impl(cstr file, u32 line, void* data, u64 size_bytes) {
        nk::mem::MemorySystem::native_allocation(file, line, size_bytes, mem::AllocationType::Free);
        free(data);
    }

#endif
}
