#include "nkpch.h"

#include "memory/allocator.h"

#include "system/memory.h"

namespace nk {
    Allocator::Allocator()
        : m_size_bytes{0},
          m_used_bytes{0},
          m_allocation_count{0},
          m_data{nullptr} {
    }

    Allocator::~Allocator() {
// #if !defined(NK_RELEASE)
//         if (m_allocation_count != 0 || m_used_bytes != 0) {
//             FatalLog("{} Allocator not correctly freed!", m_name);
//             NK_data_SYSTEM_LOG_REPORT();
//         }
// #endif
//         Assert(m_allocation_count == 0 && m_used_bytes == 0);
    }

    Allocator::Allocator(Allocator&& other)
        : m_size_bytes{other.m_size_bytes},
          m_used_bytes{other.m_used_bytes},
          m_allocation_count{other.m_allocation_count},
          m_data{other.m_data} {
        other.m_size_bytes = 0;
        other.m_used_bytes = 0;
        other.m_allocation_count = 0;
        other.m_data = nullptr;
    }

    Allocator& Allocator::operator=(Allocator&& other) {
        m_size_bytes = other.m_size_bytes;
        m_used_bytes = other.m_used_bytes;
        m_allocation_count = other.m_allocation_count;
        m_data = other.m_data;

        other.m_size_bytes = 0;
        other.m_used_bytes = 0;
        other.m_allocation_count = 0;
        other.m_data = nullptr;

        return *this;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    void Allocator::_init_allocator(cstr file, u32 line, cstr name, MemoryType::Value type) {
        NK_MEMORY_SYSTEM_ALLOCATOR_INIT(this, file, line, name, type);
    }

    void* Allocator::_allocate_raw_impl(cstr file, u32 line, const u64 size_bytes, const u64 alignment) {
        void* data = allocate_raw_impl(size_bytes, alignment);
        NK_MEMORY_SYSTEM_ALLOCATOR_ALLOCATE(this, file, line, size_bytes);
        return data;
    }

    void Allocator::_free_raw_impl(cstr file, u32 line, void* const data, const u64 size_bytes) {
        free_raw_impl(data, size_bytes);
        NK_MEMORY_SYSTEM_ALLOCATOR_FREE(this, file, line, size_bytes);
    }
#endif
}
