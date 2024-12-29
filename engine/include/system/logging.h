#pragma once

#define SPDLOG_USE_STD_FORMAT
#define SPDLOG_LEVEL_NAMES \
    { "Trace", "Debug", "Info", "Warning", "Error", "Fatal", "None" }
#define SPDLOG_SHORT_LEVEL_NAMES \
    { "T", "D", "I", "W", "E", "F", "N" }

// #if defined(NK_PLATFORM_WINDOWS)
//     #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
// #endif

#include <spdlog/spdlog.h>

namespace nk {
    namespace LoggingLevel {
        using Value = u8;

        enum : Value {
            Trace,
            Debug,
            Info,
            Warning,
            Error,
            Fatal,
            None,
        };
    }

    class LoggingSystem {
    public:
        ~LoggingSystem() = default;

        static LoggingSystem& init();
        static void shutdown();

        spdlog::logger* get_engine_logger();
        spdlog::logger* get_client_logger();

        static LoggingSystem& get() {
            static LoggingSystem instance;
            return instance;
        }
        
    private:
        LoggingSystem() {}

        spdlog::logger* m_engine_logger;
        spdlog::logger* m_client_logger;
    };
}
