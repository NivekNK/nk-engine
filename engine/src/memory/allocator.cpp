#include "nkpch.h"

#include "memory/allocator.h"

#include "systems/memory_system.h"

namespace nk::mem {
    Allocator::Allocator()
        : size_bytes{0},
          used_bytes{0},
          allocation_count{0},
          data{nullptr} {
    }

    Allocator::~Allocator() {
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        if (allocation_count != 0 || used_bytes != 0) {
            // TODO: Add Fatal Log.
            NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();
        }
#endif
        // Assert(allocation_count == 0 && used_bytes == 0);
    }

    Allocator::Allocator(Allocator&& other)
        : size_bytes{other.size_bytes},
          used_bytes{other.used_bytes},
          allocation_count{other.allocation_count},
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
          key{other.key},
#endif
          data{other.data} {
        other.size_bytes = 0;
        other.used_bytes = 0;
        other.allocation_count = 0;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        other.key = numeric::u32_max;
#endif
        other.data = nullptr;
    }

    Allocator& Allocator::operator=(Allocator&& other) {
        size_bytes = other.size_bytes;
        used_bytes = other.used_bytes;
        allocation_count = other.allocation_count;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        key = other.key;
#endif
        data = other.data;

        other.size_bytes = 0;
        other.used_bytes = 0;
        other.allocation_count = 0;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        other.key = key;
#endif
        other.data = nullptr;

        return *this;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    void* Allocator::inner_allocate_raw(cstr file, u32 line, const u64 size_bytes, const u64 alignment) {
        void* data = allocate_raw_impl(size_bytes, alignment);
        nk::mem::MemorySystem::update_allocator(this, file, line, size_bytes, mem::AllocationType::Allocate);
        return data;
    }

    void Allocator::inner_free_raw(cstr file, u32 line, void* const data, const u64 size_bytes) {
        free_raw_impl(data, size_bytes);
        nk::mem::MemorySystem::update_allocator(this, file, line, size_bytes, mem::AllocationType::Free);
    }

    void Allocator::_allocator_init(cstr file, u32 line, cstr name, MemoryType::Value type) {
        nk::mem::MemorySystem::init_allocator(this, file, line, name, type);
    }

#endif
}
