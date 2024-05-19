#pragma once

#if defined(NK_RELEASE)
    #define NK_LOG_DEBUG_ENABLED FALSE
    #define NK_LOG_TRACE_ENABLED FALSE
#else
    #define NK_LOG_DEBUG_ENABLED TRUE
    #define NK_LOG_TRACE_ENABLED TRUE
#endif

#define NK_LOG_INFO_ENABLED TRUE
#define NK_LOG_WARN_ENABLED TRUE

#define NK_DEFINE_LOG(log_name)                                                                                                        \
    template <typename... Args>                                                                                                        \
    void log_name(str file, const u32 line, cstr fmt, Args&&... args) {                                                                \
        str message;                                                                                                                   \
        std::vformat_to(                                                                                                               \
            std::back_inserter(message),                                                                                               \
            fmt,                                                                                                                       \
            std::make_format_args(std::forward<Args>(args)...));                                                                       \
        formatted_log(file, line, m_style[static_cast<u8>(nk::LoggingPriority::log_name)], message.c_str(), #log_name);                \
    }                                                                                                                                  \
                                                                                                                                       \
    template <typename Arg>                                                                                                            \
    void log_name(str file, const u32 line, Arg& arg) {                                                                                \
        formatted_log(file, line, m_style[static_cast<u8>(nk::LoggingPriority::log_name)], std::format("{}", arg).c_str(), #log_name); \
    }

namespace nk {
    struct Color {
        u8 r;
        u8 g;
        u8 b;
    };

    struct LoggingColor {
        Color fg;
        std::optional<Color> bg = std::nullopt;
    };

    static constexpr Color rgb(const u8 r, const u8 g, const u8 b) {
        return Color{r, g, b};
    }

    enum class LoggingPriority : u8 {
        Debug,
        Trace,
        Info,
        Warn,
        Error,
        Fatal,
    };

    class LoggingSystem {
    public:
        static LoggingSystem& init() { return get(); }
        static void shutdown() {}

        static LoggingSystem& get() {
            static LoggingSystem instance;
            return instance;
        }

        inline LoggingSystem& set_color(const LoggingPriority priority, LoggingColor& color) {
            m_style[static_cast<u8>(priority)] = color;
            return *this;
        }

        inline LoggingSystem& set_priority(const LoggingPriority priority) {
            m_priority = priority;
            return *this;
        }

        inline LoggingSystem& set_show_path(const bool enabled) {
            m_show_path = enabled;
            return *this;
        }

        inline LoggingSystem& set_short_path(const bool enabled) {
            m_short_path = enabled;
            return *this;
        }

        inline LoggingSystem& set_file_output(const bool enabled) {
            m_file_output = enabled;
            return *this;
        }

        inline std::mutex& get_mutex() { return m_log_mutex; }

        template <typename... Args>
        void Styled(str file, const u32 line, const LoggingColor& color, cstr fmt, Args&&... args) {
            str message;
            std::vformat_to(
                std::back_inserter(message),
                fmt,
                std::make_format_args(std::forward<Args>(args)...));
            formatted_log(file, line, color, message.c_str());
        }

        template <typename Arg>
        void Styled(str file, const u32 line, const LoggingColor& color, Arg& arg) {
            formatted_log(file, line, color, std::format("{}", arg).c_str());
        }

        template <typename... Args>
        void Styled(str file, const u32 line, const Color& foreground, cstr fmt, Args&&... args) {
            str message;
            std::vformat_to(
                std::back_inserter(message),
                fmt,
                std::make_format_args(std::forward<Args>(args)...));
            formatted_log(file, line, {.fg = foreground}, message.c_str());
        }

        template <typename Arg>
        void Styled(str file, const u32 line, const Color& foreground, Arg& arg) {
            formatted_log(file, line, {.fg = foreground}, std::format("{}", arg).c_str());
        }

        NK_DEFINE_LOG(Debug)
        NK_DEFINE_LOG(Trace)
        NK_DEFINE_LOG(Info)
        NK_DEFINE_LOG(Warn)
        NK_DEFINE_LOG(Error)
        NK_DEFINE_LOG(Fatal)

    private:
        LoggingSystem();

        void formatted_log(str file, const u32 line, const LoggingColor& color, cstr message, cstr log_name = nullptr);

        LoggingColor m_style[static_cast<u8>(LoggingPriority::Fatal) + 1];
        LoggingPriority m_priority;
        bool m_show_path;
        bool m_short_path;
        bool m_file_output;

        std::mutex m_log_mutex;

        static constexpr LoggingColor s_default_style[static_cast<u8>(LoggingPriority::Fatal) + 1] = {
            {.fg = rgb(166, 226, 46)},                           // Debug
            {.fg = rgb(170, 129, 246)},                          // Trace
            {.fg = rgb(255, 255, 255)},                          // Info
            {.fg = rgb(255, 128, 0)},                            // Warn
            {.fg = rgb(233, 38, 109)},                           // Error
            {.fg = rgb(255, 255, 255), .bg = rgb(233, 38, 109)}, // Fatal
        };
    };
}

#if NK_LOG_DEBUG_ENABLED
    #define DebugLog(...) ::nk::LoggingSystem::get().Debug(__FILE__, __LINE__, __VA_ARGS__)
    #define DebugLogIf(value, ...) \
        if (value)                 \
        ::nk::LoggingSystem::get().Debug(__FILE__, __LINE__, __VA_ARGS__)
#else
    #define DebugLog(...)
    #define DebugIfLog(value, ...)
#endif

#if NK_LOG_TRACE_ENABLED
    #define TraceLog(...) ::nk::LoggingSystem::get().Trace(__FILE__, __LINE__, __VA_ARGS__)
    #define TraceLogIf(value, ...) \
        if (value)                 \
        ::nk::LoggingSystem::get().Trace(__FILE__, __LINE__, __VA_ARGS__)
#else
    #define TraceLog(...)
    #define TraceIfLog(value, ...)
#endif

#if NK_LOG_INFO_ENABLED
    #define InfoLog(...) ::nk::LoggingSystem::get().Info(__FILE__, __LINE__, __VA_ARGS__)
    #define InfoLogIf(value, ...) \
        if (value)                \
        ::nk::LoggingSystem::get().Info(__FILE__, __LINE__, __VA_ARGS__)
#else
    #define InfoLog(...)
    #define InfoIfLog(value, ...)
#endif

#if NK_LOG_WARN_ENABLED
    #define WarnLog(...) ::nk::LoggingSystem::get().Warn(__FILE__, __LINE__, __VA_ARGS__)
    #define WarnLogIf(value, ...) \
        if (value)                \
        ::nk::LoggingSystem::get().Warn(__FILE__, __LINE__, __VA_ARGS__)
#else
    #define WarnLog(...)
    #define WarnIfLog(value, ...)
#endif

#define ErrorLog(...) ::nk::LoggingSystem::get().Error(__FILE__, __LINE__, __VA_ARGS__)
#define ErrorLogIf(value, ...) \
    if (value)                 \
    ::nk::LoggingSystem::get().Error(__FILE__, __LINE__, __VA_ARGS__)

#define FatalLog(...) ::nk::LoggingSystem::get().Fatal(__FILE__, __LINE__, __VA_ARGS__)
#define FatalLogIf(value, ...) \
    if (value)                 \
    ::nk::LoggingSystem::get().Fatal(__FILE__, __LINE__, __VA_ARGS__)

#define StyledLog(color, ...) ::nk::LoggingSystem::get().Styled(__FILE__, __LINE__, color, __VA_ARGS__)
