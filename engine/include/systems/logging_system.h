#pragma once

#include "vendor/glm/color.h"

namespace nk {
    enum class LoggingLevel : u8 {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Fatal,
        None,
        Off,
    };

    struct LoggingColor {
        glm::color fg;
        std::optional<glm::color> bg = std::nullopt;
    };

    struct LoggingSystemConfig {
        LoggingColor style[static_cast<u8>(LoggingLevel::Off)];
        LoggingLevel priority;
        bool show_file;
        bool show_time;
        bool file_output;
    };

    class LoggingSystem {
    public:
        ~LoggingSystem() = default;

        static LoggingSystem& init(const LoggingSystemConfig& config = get_default_config());
        static void shutdown();

        static LoggingSystem& get() {
            static LoggingSystem instance;
            return instance;
        }

        static LoggingSystemConfig get_default_config() {
            LoggingSystemConfig config = {};
            std::copy(std::begin(default_style), std::end(default_style), config.style);
            config.priority = LoggingLevel::Trace;
            config.show_file = true;
            config.show_time = true;
            config.file_output = true;
            return config;
        }

        template <typename... Args>
        static void log(LoggingLevel level, std::string_view file, u32 line, std::string_view fmt, Args&&... args) {
            std::string buffer;
            std::vformat_to(std::back_inserter(buffer), fmt, std::make_format_args(args...));
            log(level, file, line, buffer);
        }

        static void log(LoggingLevel level, std::string_view file, u32 line, std::string_view message);

    private:
        LoggingSystem() = default;

        static constexpr LoggingColor default_style[static_cast<u8>(LoggingLevel::Off)] = {
            {.fg = rgb(170, 129, 246)},                          // Trace
            {.fg = rgb(166, 226, 46)},                           // Debug
            {.fg = rgb(255, 255, 255)},                          // Info
            {.fg = rgb(255, 128, 0)},                            // Warn
            {.fg = rgb(233, 38, 109)},                           // Error
            {.fg = rgb(255, 255, 255), .bg = rgb(233, 38, 109)}, // Fatal
            {.fg = rgb(218, 218, 218)},                          // None
        };

        struct LoggingLevelString {
            cstr value;
            size_t size;
        };

        static constexpr LoggingLevelString logging_level[static_cast<u8>(LoggingLevel::None)] = {
            { .value = " [Trace]: ", .size = 10 },
            { .value = " [Debug]: ", .size = 10 },
            { .value = " [Info]: ", .size = 9 },
            { .value = " [Warning]: ", .size = 12 },
            { .value = " [Error]: ", .size = 10 },
            { .value = " [Fatal]: ", .size = 10 },
        };

        std::string style[static_cast<u8>(LoggingLevel::Off)];
        LoggingLevel priority;
        bool show_file;
        bool show_time;
        bool file_output;
    };
}
