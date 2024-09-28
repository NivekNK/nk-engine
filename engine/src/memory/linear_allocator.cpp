#include "nkpch.h"

#include "memory/linear_allocator.h"

#include "system/memory_system.h"

namespace nk {
    LinearAllocator::LinearAllocator() : Allocator() {
        NK_MEMORY_SYSTEM_INSERT(*this);
    }

    void* LinearAllocator::allocate_raw_impl(const u64 size_bytes, const u64 alignment) {
        if (m_used_bytes + size_bytes > m_size_bytes) {
            const u64 remaining_bytes = m_size_bytes - m_used_bytes;
            ErrorLog("LinearAllocator::allocate_raw_impl tried to allocate {}B, only {}B remaining.", size_bytes, remaining_bytes);
            return nullptr;
        }

        void* block = static_cast<void*>(static_cast<char*>(m_start) + m_used_bytes);
        m_used_bytes += size_bytes;
        m_allocation_count++;
        return block;
    }

    void LinearAllocator::free_all() {
#if !defined(NK_RELEASE)
        u64 aux_size = m_size_bytes;
#endif
        m_allocation_count = 0;
        m_size_bytes = 0;
        m_used_bytes = 0;
        std::free(m_start);
        m_start = nullptr;
        NK_MEMORY_SYSTEM_UPDATE(*this, aux_size, __FILE__, __LINE__, AllocationType::Free);
    }
}
