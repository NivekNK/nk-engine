#include "nkpch.h"

#include "core/os.h"

#include "systems/memory_system.h"

namespace nk::os {
    void* _native_allocate(u64 size_bytes, u64 alignment) {
        return malloc(size_bytes);
    }

    void _native_free(void* data, u64 size_bytes) {
        free(data);
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    void* _native_allocate(cstr file, u32 line, u64 size_bytes, u64 alignment) {
        void* data = malloc(size_bytes);
        mem::MemorySystem::native_allocation(file, line, data, size_bytes, mem::AllocationType::Allocate);
        return data;
    }

    void _native_free(cstr file, u32 line, void* data, u64 size_bytes) {
        mem::MemorySystem::native_allocation(file, line, data, size_bytes, mem::AllocationType::Free);
        free(data);
    }

#endif
}
