#pragma once

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #include "core/os.h"

namespace nk {
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

        static void native_allocation(cstr file, u32 line, u64 size_bytes,
                                      AllocationType allocation_type);

        static void log_report(bool detailed = false);

    private:
        MemorySystem() = default;

        void log_title(std::string_view msg) {
            log("\033[38;2;170;129;246m", msg.data(), msg.length());
        }

        template <typename... Args>
        void log_text(std::string_view fmt, Args&&... args) {
            std::string buffer;
            std::vformat_to(std::back_inserter(buffer), fmt, std::make_format_args(args...));
            log("\033[38;2;255;255;255m", buffer.c_str(), buffer.size());
        }

        void log(cstr color, cstr msg, size_t msg_size);

        std::mutex mutex;
        void* data;

        friend void* get_memory_system_data();
    };
}

    #define NK_MEMORY_SYSTEM_INIT() \
        nk::MemorySystem::init()
    #define NK_MEMORY_SYSTEM_SHUTDOWN() \
        nk::MemorySystem::shutdown()
    #define NK_MEMORY_SYSTEM_LOG_REPORT() \
        nk::MemorySystem::log_report()
    #define NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT() \
        nk::MemorySystem::log_report(true)

#else

    #define NK_MEMORY_SYSTEM_INIT()
    #define NK_MEMORY_SYSTEM_SHUTDOWN()
    #define NK_MEMORY_SYSTEM_LOG_REPORT()
    #define NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT()

#endif