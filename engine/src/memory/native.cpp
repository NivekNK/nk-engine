#include "nkpch.h"

#include "system/memory_system.h"

namespace nk::mem {
    void* allocate_impl(cstr file, const u64 line, const u64 size_bytes) {
        void* memory = std::malloc(size_bytes);
        NK_MEMORY_SYSTEM_UPDATE_NATIVE(size_bytes, file, line, AllocationType::Allocate);
        return memory;
    }

    void free_impl(cstr file, const u64 line, const u64 size_bytes, void* ptr) {
        free(ptr);
        NK_MEMORY_SYSTEM_UPDATE_NATIVE(size_bytes, file, line, AllocationType::Free);
    }

    void* allocate_impl(const u64 size_bytes) {
        return std::malloc(size_bytes);
    }

    void free_impl(void* ptr) {
        free(ptr);
    }
}
