#include "nkpch.h"

#include "memory/allocator.h"

#include "system/memory_system.h"

namespace nk {
    Allocator::Allocator()
        : m_size_bytes{0},
          m_used_bytes{0},
          m_allocation_count{0},
          m_start{nullptr} {
    }

    Allocator::~Allocator() {
#if !defined(NK_RELEASE) && !defined(NK_TESTING)
        if (m_allocation_count != 0 || m_used_bytes != 0) {
            FatalLog("{} Allocator not correctly freed!", m_name);
            NK_MEMORY_SYSTEM_LOG_REPORT();
        }
#endif
        Assert(m_allocation_count == 0 && m_used_bytes == 0);
    }

    Allocator::Allocator(Allocator&& other)
        : m_size_bytes{other.m_size_bytes},
          m_used_bytes{other.m_used_bytes},
          m_allocation_count{other.m_allocation_count},
#if !defined(NK_RELEASE) && !defined(NK_TESTING)
          m_start{other.m_start},
          m_name{other.m_name},
          m_type{other.m_type}
#else
          m_start{other.m_start}
#endif
    {
        other.m_size_bytes = 0;
        other.m_used_bytes = 0;
        other.m_allocation_count = 0;
        other.m_start = nullptr;
#if !defined(NK_RELEASE) && !defined(NK_TESTING)
        other.m_name = "None";
        other.m_type = MemoryType::None;
#endif
    }

    Allocator& Allocator::operator=(Allocator&& rhs) {
        m_size_bytes = rhs.m_size_bytes;
        m_used_bytes = rhs.m_used_bytes;
        m_allocation_count = rhs.m_allocation_count;
        m_start = rhs.m_start;
#if !defined(NK_RELEASE) && !defined(NK_TESTING)
        m_name = rhs.m_name;
        m_type = rhs.m_type;
#endif

        rhs.m_size_bytes = 0;
        rhs.m_used_bytes = 0;
        rhs.m_allocation_count = 0;
        rhs.m_start = nullptr;
#if !defined(NK_RELEASE) && !defined(NK_TESTING)
        rhs.m_name = "None";
        rhs.m_type = MemoryType::None;
#endif

        return *this;
    }

#if !defined(NK_RELEASE) && !defined(NK_TESTING)
    void* Allocator::memory_manager_allocate_raw_impl(
        const u64 size_bytes, const u64 alignment, str file, const u32 line) {
        auto memory = allocate_raw_impl(size_bytes, alignment);
        NK_MEMORY_SYSTEM_UPDATE(*this, size_bytes, file, line, AllocationType::Allocate);
        return memory;
    }

    void Allocator::memory_manager_free_raw_impl(
        void* const ptr, const u64 size_bytes, str file, const u32 line) {
        free_raw_impl(ptr, size_bytes);
        NK_MEMORY_SYSTEM_UPDATE(*this, size_bytes, file, line, AllocationType::Free);
    }
#endif

#if !defined(NK_RELEASE) && !defined(NK_TESTING)
    void Allocator::private_allocator_init(
        const u64 size_bytes, void* start, str name, str file, const u32 line, const MemoryTypeValue memory_type) {
        m_size_bytes = size_bytes;
        m_start = start;
        m_name = name;
        m_type = memory_type;
        NK_MEMORY_SYSTEM_UPDATE(*this, size_bytes, file, line, AllocationType::Init);
    }
#else
    void Allocator::private_allocator_init(const u64 size_bytes, void* start) {
        m_size_bytes = size_bytes;
        m_start = start;
    }
#endif
}
