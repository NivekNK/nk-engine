#pragma once

#include "memory/memory_type.h"

namespace nk {
#if !defined(NK_RELEASE)
    struct AllocatorInitInfo {
        str file = "";
        u64 line = 0;
        u64 index = -1;
        str name = "undefined";
        MemoryTypeValue type = MemoryType::None;
    };
#endif

    class Allocator {
    public:
        Allocator();
        virtual ~Allocator();

#if defined(NK_RELEASE)

        virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) = 0;
        virtual void free_raw_impl(void* const ptr, const u64 size_bytes) = 0;

#else

        virtual void* allocate_raw_impl(cstr file, const u64 line, const u64 size_bytes, const u64 alignment) = 0;
        virtual void free_raw_impl(cstr file, const u64 line, void* const ptr, const u64 size_bytes) = 0;

        AllocatorInitInfo& get_init_info() { return m_init_info; }

#endif

    protected:
        u64 m_size_bytes;
        u64 m_used_bytes;
        u64 m_allocation_count;
        void* m_memory;

#if !defined(NK_RELEASE)
        AllocatorInitInfo m_init_info;
#endif
    };
}

#if !defined(NK_RELEASE)
    #define NK_DEFINE_ALLOCATE_RAW_H() \
        virtual void* allocate_raw_impl(cstr file, const u64 line, const u64 size_bytes, const u64 alignment) override
#else
    #define NK_DEFINE_ALLOCATE_RAW() \
        virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) override
#endif

#if !defined(NK_RELEASE)
    #define NK_DEFINE_ALLOCATE_RAW_CPP()
#else
#endif
