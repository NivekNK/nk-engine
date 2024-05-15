#pragma once

#if !defined(NK_RELEASE)

    #include "memory/memory_type.h"
    #include "core/dyarr.h"

namespace nk {
    class Allocator;

    enum class AllocationType {
        Init,
        Allocate,
        Free,
    };

    struct MemorySystemAllocationInfo {
        u64 size_bytes;
        str file;
        u32 line;
        AllocationType type;
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
        static void init();
        static void shutdown();

        void expanded_memory_type(u64 max_memory_type, const std::function<cstr(MemoryTypeValue)>& memory_type_to_cstr);
        void insert(Allocator& allocator);
        void update(const Allocator& allocator, const u64 size_bytes, str file, const u32 line, AllocationType type);
        void log_report(bool detailed);

        inline static bool is_initialized() {
            return s_instance != nullptr;
        }

        inline static MemorySystem& get() {
            AssertMsg(s_instance != nullptr, "Trying to get MemorySystem when not initialized!");
            return *s_instance;
        }

    private:
        MemorySystem();

        Allocator* m_allocator;
        Dyarr<MemorySystemStats> m_allocations;

        u64 m_max_memory_type;
        std::function<cstr(MemoryTypeValue)> m_memory_type_to_cstr;

        static MemorySystem* s_instance;
    };
}

    #define NK_MEMORY_SYSTEM_INIT() \
        ::nk::MemorySystem::init()
    #define NK_MEMORY_SYSTEM_SHUTDOWN() \
        ::nk::MemorySystem::shutdown()
    #define NK_MEMORY_SYSTEM_IS_INITIALIZED() \
        ::nk::MemorySystem::is_initialized()
    #define NK_MEMORY_SYSTEM_EXPANDED_MEMORY_TYPE(max_memory_type, memory_type_to_cstr) \
        if (nk::MemorySystem::is_initialized()) nk::MemorySystem::get().expanded_memory_type(max_memory_type, memory_type_to_cstr)
    #define NK_MEMORY_SYSTEM_INSERT(allocator)  \
        if (nk::MemorySystem::is_initialized()) nk::MemorySystem::get().insert(allocator)
    #define NK_MEMORY_SYSTEM_UPDATE(allocator, size_bytes, file, line, type) \
        if (nk::MemorySystem::is_initialized()) nk::MemorySystem::get().update(allocator, size_bytes, file, line, type)
    #define NK_MEMORY_SYSTEM_LOG_REPORT() \
        if (nk::MemorySystem::is_initialized()) nk::MemorySystem::get().log_report(false)
    #define NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT() \
        if (nk::MemorySystem::is_initialized()) nk::MemorySystem::get().log_report(true)

#else

    #define NK_MEMORY_SYSTEM_INIT()
    #define NK_MEMORY_SYSTEM_SHUTDOWN()
    #define NK_MEMORY_SYSTEM_IS_INITIALIZED() false
    #define NK_MEMORY_SYSTEM_EXPANDED_MEMORY_TYPE(max_memory_type, memory_type_to_cstr)
    #define NK_MEMORY_SYSTEM_INSERT(allocator)
    #define NK_MEMORY_SYSTEM_UPDATE(allocator, size_bytes, file, line, type)
    #define NK_MEMORY_SYSTEM_LOG_REPORT()
    #define NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT()

#endif
