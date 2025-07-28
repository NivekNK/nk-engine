#include "nkpch.h"

#include "memory/allocator.h"

#include "systems/memory_system.h"

namespace nk::mem {
    Allocator::Allocator()
        : m_size_bytes{0},
          m_used_bytes{0},
          m_allocation_count{0},
          m_data{nullptr} {
    }

    Allocator::~Allocator() {
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        if (m_allocation_count != 0 || m_used_bytes != 0) {
            NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();
        }
#endif
        Assert(m_allocation_count == 0 && m_used_bytes == 0);
    }

    Allocator::Allocator(Allocator&& other)
        : m_size_bytes{other.m_size_bytes},
          m_used_bytes{other.m_used_bytes},
          m_allocation_count{other.m_allocation_count},
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
          m_key{other.m_key},
#endif
          m_data{other.m_data} {
        other.m_size_bytes = 0;
        other.m_used_bytes = 0;
        other.m_allocation_count = 0;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        other.m_key = numeric::u32_max;
#endif
        other.m_data = nullptr;
    }

    Allocator& Allocator::operator=(Allocator&& other) {
        m_size_bytes = other.m_size_bytes;
        m_used_bytes = other.m_used_bytes;
        m_allocation_count = other.m_allocation_count;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        m_key = other.m_key;
#endif
        m_data = other.m_data;

        other.m_size_bytes = 0;
        other.m_used_bytes = 0;
        other.m_allocation_count = 0;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        other.m_key = numeric::u32_max;
#endif
        other.m_data = nullptr;

        return *this;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    void* Allocator::_allocate_raw(cstr file, u32 line, const u64 size_bytes, const u64 alignment) {
        void* data = _allocate_raw(size_bytes, alignment);
        mem::MemorySystem::update_allocator(this, file, line, data, size_bytes, mem::AllocationType::Allocate);
        return data;
    }

    void Allocator::_free_raw(cstr file, u32 line, void* const data, const u64 size_bytes) {
        mem::MemorySystem::update_allocator(this, file, line, data, size_bytes, mem::AllocationType::Free);
        _free_raw(data, size_bytes);
    }

    std::string_view Allocator::_allocator_name() {
        return mem::MemorySystem::get_allocator_name(this);
    }

    void Allocator::_allocator_init(cstr file, u32 line, cstr name, MemoryType::Value type) {
        mem::MemorySystem::init_allocator(this, file, line, name, type);
    }

#endif
}
