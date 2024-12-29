#pragma once

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #include "memory/allocator.h"

    #include <string>
    #include <vector>
    #include <memory>

    // TODO: Disable default logger
    #define SPDLOG_USE_STD_FORMAT
    #include <spdlog/spdlog.h>

namespace nk {
    namespace MemorySystemInternal {
        namespace AllocationType {
            using Value = u8;

            enum : u8 {
                Init,
                Allocate,
                Free,
            };

            inline cstr to_cstr(const AllocationType::Value type) {
                static constexpr cstr types[] = {"Init", "Allocate", "Free"};
                return types[type];
            }
        }

        struct AllocatorInfo {
            u64 size_bytes;
            std::string file;
            u32 line;
            AllocationType::Value type;
        };

        struct AllocationStats {
            std::string name;
            std::string allocator;
            MemoryType::Value type;

            u64 size_bytes;
            u64 used_bytes;
            u64 allocation_count;

            std::vector<AllocatorInfo> allocator_log;
        };
    }

    class MemorySystem {
    public:
        ~MemorySystem() = default;

        static MemorySystem& init();
        static void shutdown();

        static MemorySystem& get() {
            static MemorySystem instance;
            return instance;
        }

        static void init_allocator(
            Allocator* allocator, cstr file, u32 line, cstr name, MemoryType::Value type);
        static void update_allocator(
            Allocator* allocator, cstr file, u32 line, u64 size_bytes,
            MemorySystemInternal::AllocationType::Value allocation_type);

        static void log_report(bool detailed = false);

    private:
        MemorySystem() {}

        std::vector<MemorySystemInternal::AllocationStats> m_allocations;
        std::shared_ptr<spdlog::logger> m_logger;
    };
}

    #define NK_MEMORY_SYSTEM_INIT() \
        nk::MemorySystem::init()
    #define NK_MEMORY_SYSTEM_SHUTDOWN() \
        nk::MemorySystem::shutdown()
    #define NK_MEMORY_SYSTEM_ALLOCATOR_INIT(allocator, file, line, name, type) \
        nk::MemorySystem::init_allocator(allocator, file, line, name, type)
    #define NK_MEMORY_SYSTEM_ALLOCATOR_ALLOCATE(allocator, file, line, size_bytes)                \
        nk::MemorySystem::update_allocator(allocator, file, line, size_bytes, \
                                           nk::MemorySystemInternal::AllocationType::Allocate)
    #define NK_MEMORY_SYSTEM_ALLOCATOR_FREE(allocator, file, line, size_bytes)                    \
        nk::MemorySystem::update_allocator(allocator, file, line, size_bytes, \
                                           nk::MemorySystemInternal::AllocationType::Free)
    #define NK_MEMORY_SYSTEM_LOG_REPORT() \
        nk::MemorySystem::log_report()
    #define NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT() \
        nk::MemorySystem::log_report(true)

#else

    #define NK_MEMORY_SYSTEM_INIT()
    #define NK_MEMORY_SYSTEM_SHUTDOWN()
    #define NK_MEMORY_SYSTEM_ALLOCATOR_ALLOCATE(allocator, size_bytes)
    #define NK_MEMORY_SYSTEM_ALLOCATOR_FREE(allocator, size_bytes)
    #define NK_MEMORY_SYSTEM_LOG_REPORT()
    #define NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT()

#endif
