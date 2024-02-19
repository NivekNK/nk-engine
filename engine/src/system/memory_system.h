#pragma once

#if !defined(NK_RELEASE)

#include "memory/memory_type.h"
#include "core/dyarr.h"

namespace nk {
    class Allocator;

    struct MemorySystemAllocationInfo {
        u64 size_bytes;
        str file;
        u32 line;
        bool is_allocate;
    };

    struct MemorySystemStats {
        str name;
        str allocator;
        MemoryTypeValue type;

        u64 size_bytes;
        u64 used_bytes;
        u64 allocation_count;

        Dyarr<MemorySystemAllocationInfo> allocation_log;
    };

    class MemorySystem {
    public:
        ~MemorySystem();

        static void init();
        static void shutdown();

        void insert(const Allocator& allocator);
        void allocate_memory(const Allocator& allocator, const u64 size_bytes, str file, const u32 line);
        void free_memory(const Allocator& allocator, const u64 size_bytes, str file, const u32 line);
        void report();

        inline static MemorySystem& get() {
            return *s_instance;
        }

    private:
        MemorySystem();

        Allocator* m_allocator;

        static MemorySystem* s_instance;
    };
}

    #define NK_MEMORY_SYSTEM_INIT() \
        ::nk::MemorySystem::init()
    #define NK_MEMORY_SYSTEM_SHUTDOWN() \
        ::nk::MemorySystem::shutdown()
    #define NK_MEMORY_SYSTEM_INSERT(allocator) \
        ::nk::MemorySystem::get().insert(allocator)
    #define NK_MEMORY_SYSTEM_ALLOCATE(allocator, size_bytes, file, line) \
        ::nk::MemorySystem::get().allocate_memory(allocator, size_bytes, file, line)
    #define NK_MEMORY_SYSTEM_FREE(allocator, size_bytes, file, line) \
        ::nk::MemorySystem::get().free_memory(allocator, size_bytes, file, line)
    #define NK_MEMORY_SYSTEM_REPORT() \
        ::nk::MemorySystem::get().report()

#else

    #define NK_MEMORY_SYSTEM_INIT()
    #define NK_MEMORY_SYSTEM_SHUTDOWN()
    #define NK_MEMORY_SYSTEM_INSERT(allocator)
    #define NK_MEMORY_SYSTEM_ALLOCATE(allocator, size_bytes, file, line)
    #define NK_MEMORY_SYSTEM_FREE(allocator, size_bytes, file, line)
    #define NK_MEMORY_SYSTEM_REPORT()

#endif
