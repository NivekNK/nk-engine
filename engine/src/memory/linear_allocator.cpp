#include "nkpch.h"

#include "memory/linear_allocator.h"

#include "systems/memory_system.h"

namespace nk::mem {
    LinearAllocator::LinearAllocator()
        : Allocator(),
          m_owns_memory{false} {}

    LinearAllocator::~LinearAllocator() {
        if (m_owns_memory && m_data != nullptr) {
            native_free(m_data, m_size_bytes);
        }
    }

    LinearAllocator::LinearAllocator(LinearAllocator&& other)
        : Allocator(std::move(other)),
          m_owns_memory{other.m_owns_memory} {
        other.m_owns_memory = false;
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) {
        Allocator::operator=(std::move(other));
        m_owns_memory = other.m_owns_memory;
        other.m_owns_memory = false;
        return *this;
    }

    void LinearAllocator::init(u64 size_bytes, void* data) {
        Assert(size_bytes > 0, "Linear Allocator initialize size_bytes needs to be more than zero.");
        m_size_bytes = size_bytes;
        m_owns_memory = data == nullptr;
        if (m_owns_memory) {
            m_data = std::calloc(1, size_bytes);
        } else {
            m_data = data;
        }
    }

    void* LinearAllocator::_allocate_raw(const u64 size_bytes, [[maybe_unused]] const u64 alignment) {
        if (m_data == nullptr) {
            ErrorLog("nk::mem::LinealAllocator tried to allocate not initialized.");
            return nullptr;
        }

        const u64 used_bytes = m_used_bytes + size_bytes;
        if (used_bytes > m_size_bytes) {
            ErrorLog("nk::mem::LinearAllocator tried to allocate {}B, only {}B remaining.", size_bytes, m_size_bytes - m_used_bytes);
            return nullptr;
        }

        void* block = static_cast<u8*>(m_data) + m_size_bytes;
        m_allocation_count++;
        m_used_bytes = used_bytes;
        return block;
    }

    bool LinearAllocator::_free_linear_allocator() {
        if (m_size_bytes <= 0)
            return false;

        m_allocation_count = 0;
        m_used_bytes = 0;
        std::memset(m_data, 0, m_size_bytes);

        return true;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    bool LinearAllocator::_free_linear_allocator(cstr file, u32 line) {
        u64 size_bytes = m_size_bytes;
        void* const data = m_data;
        bool freed = _free_linear_allocator();
        if (freed)
            mem::MemorySystem::update_allocator(this, file, line, data, size_bytes, mem::AllocationType::Free);
        return freed;
    }
#endif
}
