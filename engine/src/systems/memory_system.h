#pragma once

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #include "memory/memory_type.h"

namespace nk::mem {
    class Allocator;

    enum class AllocationType : u8 {
        Init,
        Allocate,
        Free,
    };

    class MemorySystem {
    public:
        ~MemorySystem() = default;

        static MemorySystem& init();
        static void shutdown();

        static MemorySystem& get() {
            static MemorySystem instance;
            return instance;
        }

        static void native_allocation(cstr file, u32 line, void* data, u64 size_bytes,
                                      AllocationType allocation_type);
        static void init_allocator(mem::Allocator* allocator, cstr file,
                                   u32 line, cstr name, MemoryType::Value type);
        static void update_allocator(mem::Allocator* allocator, cstr file,
                                     u32 line, void* data, u64 size_bytes,
                                     AllocationType allocation_type);

        static void clear_allocator_tracking(mem::Allocator* allocator, cstr file, u32 line);

        static void log_report(bool detailed = false);
        static std::string_view get_allocator_name(mem::Allocator* allocator);

    private:
        MemorySystem() = default;

        void log_title(std::string_view msg) {
            log("\033[38;2;170;129;246m", msg.data(), msg.length());
        }

        void log_info(std::string_view msg) {
            log("\033[38;2;255;255;255m", msg.data(), msg.length());
        }

        void log_warn(std::string_view msg) {
            log("\033[38;2;255;128;0m", msg.data(), msg.length());
        }

        void log_error(std::string_view msg) {
            log("\033[38;2;233;38;109m", msg.data(), msg.length());
        }

        void log_trace(std::string_view msg) {
            log("\033[38;2;218;218;218m", msg.data(), msg.length());
        }

        void log(cstr color, cstr msg, std::size_t msg_size);

        void* m_data;

        friend void* get_memory_system_data();
    };
}

    #define NK_MEMORY_SYSTEM_INIT() \
        nk::mem::MemorySystem::init()
    #define NK_MEMORY_SYSTEM_SHUTDOWN() \
        nk::mem::MemorySystem::shutdown()
    #define NK_MEMORY_SYSTEM_LOG_REPORT() \
        nk::mem::MemorySystem::log_report()
    #define NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT() \
        nk::mem::MemorySystem::log_report(true)

#else

    #define NK_MEMORY_SYSTEM_INIT()
    #define NK_MEMORY_SYSTEM_SHUTDOWN()
    #define NK_MEMORY_SYSTEM_LOG_REPORT()
    #define NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT()

#endif
