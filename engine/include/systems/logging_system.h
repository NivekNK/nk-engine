#pragma once

#include "core/format.h"
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
            for (u8 index = 0; index < static_cast<u8>(LoggingLevel::Off); ++index)
                config.style[index] = default_style[index];
            config.priority = LoggingLevel::Trace;
            config.show_file = true;
            config.show_time = true;
            config.file_output = true;
            return config;
        }

        template <typename First, typename... Args>
        static void log(
            LoggingLevel level,
            strview file,
            u32 line,
            format_string<
                std::type_identity_t<First>,
                std::type_identity_t<Args>...> format,
            First&& first,
            Args&&... args) noexcept {
            strbuf<2048> message;
            format_to(
                message,
                format,
                std::forward<First>(first),
                std::forward<Args>(args)...);
            message.mark_truncated();
            log(level, file, line, message.view());
        }

        static void log(
            LoggingLevel level,
            strview file,
            u32 line,
            strview message) noexcept;

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
            std::size_t size;
        };

        static constexpr LoggingLevelString logging_level[static_cast<u8>(LoggingLevel::None)] = {
            {.value = " [Trace]: ", .size = 10},
            {.value = " [Debug]: ", .size = 10},
            {.value = " [Info]: ", .size = 9},
            {.value = " [Warning]: ", .size = 12},
            {.value = " [Error]: ", .size = 10},
            {.value = " [Fatal]: ", .size = 10},
        };

        strbuf<64> m_style[static_cast<u8>(LoggingLevel::Off)];
        LoggingLevel m_priority = LoggingLevel::Trace;
        bool m_show_file = true;
        bool m_show_time = true;
        bool m_file_output = false;
    };
}

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
    #define NK_LOG_TRACE_ENABLED TRUE
    #define NK_LOG_DEBUG_ENABLED TRUE
#else
    #define NK_LOG_TRACE_ENABLED FALSE
    #define NK_LOG_DEBUG_ENABLED FALSE
#endif

#define NK_LOG_INFO_ENABLED TRUE
#define NK_LOG_WARN_ENABLED TRUE

#if NK_LOG_TRACE_ENABLED
    #define TraceLog(...) nk::LoggingSystem::log(nk::LoggingLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
    #define TraceLogIf(condition, ...) \
        if (condition)                 \
        nk::LoggingSystem::log(nk::LoggingLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define TraceLog(...)
    #define TraceLogIf(condition, ...)
#endif

#if NK_LOG_DEBUG_ENABLED
    #define DebugLog(...) nk::LoggingSystem::log(nk::LoggingLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
    #define DebugLogIf(condition, ...) \
        if (condition)                 \
        nk::LoggingSystem::log(nk::LoggingLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define DebugLog(...)
    #define DebugLogIf(condition, ...)
#endif

#if NK_LOG_INFO_ENABLED
    #define InfoLog(...) nk::LoggingSystem::log(nk::LoggingLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
    #define InfoLogIf(condition, ...) \
        if (condition)                \
        nk::LoggingSystem::log(nk::LoggingLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define InfoLog(...)
    #define InfoLogIf(condition, ...)
#endif

#if NK_LOG_WARN_ENABLED
    #define WarnLog(...) nk::LoggingSystem::log(nk::LoggingLevel::Warning, __FILE__, __LINE__, __VA_ARGS__)
    #define WarnLogIf(condition, ...) \
        if (condition)                \
        nk::LoggingSystem::log(nk::LoggingLevel::Warning, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define WarnLog(...)
    #define WarnLogIf(condition, ...)
#endif

#define ErrorLog(...) nk::LoggingSystem::log(nk::LoggingLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define ErrorLogIf(condition, ...) \
    if (condition)                 \
    nk::LoggingSystem::log(nk::LoggingLevel::Error, __FILE__, __LINE__, __VA_ARGS__)

#define FatalLog(...) nk::LoggingSystem::log(nk::LoggingLevel::Fatal, __FILE__, __LINE__, __VA_ARGS__)
#define FatalLogIf(condition, ...) \
    if (condition)                 \
    nk::LoggingSystem::log(nk::LoggingLevel::Fatal, __FILE__, __LINE__, __VA_ARGS__)
