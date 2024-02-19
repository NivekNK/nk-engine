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

namespace nk {
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
        static void init() { get(); }
        static void shutdown() {}

        static LoggingSystem& get() {
            static LoggingSystem instance;
            return instance;
        }

        ~LoggingSystem() = default;

        inline LoggingSystem& set_priority(const LoggingPriority priority) {
            m_priority = priority;
            return *this;
        }
        inline LoggingSystem& set_file_output(const bool enabled) {
            m_file_output = enabled;
            return *this;
        }

        void styled_log(
            const cstr log_name,
            const cstr message,
            i8 font, i8 background, i8 style,
            const cstr file,
            const u32 line);


        template <typename... Args>
        void Debug(const cstr file, const u32 line, cstr fmt, Args&&... args) {
            if (m_priority > LoggingPriority::Debug)
                return;
            str message;
            std::vformat_to(
                std::back_inserter(message),
                fmt,
                std::make_format_args(std::forward<Args>(args)...));
            constexpr u8 priority_index = static_cast<u8>(LoggingPriority::Debug);
            styled_log("Debug", message.c_str(), m_style[priority_index][0], m_style[priority_index][1], m_style[priority_index][2], file, line);
        }

        template <typename... Args>
        void Trace(const cstr file, const u32 line, cstr fmt, Args&&... args) {
            if (m_priority > LoggingPriority::Trace)
                return;
            str message;
            std::vformat_to(
                std::back_inserter(message),
                fmt,
                std::make_format_args(std::forward<Args>(args)...));
            constexpr u8 priority_index = static_cast<u8>(LoggingPriority::Trace);
            styled_log("Trace", message.c_str(), m_style[priority_index][0], m_style[priority_index][1], m_style[priority_index][2], file, line);
        }

        template <typename... Args>
        void Info(const cstr file, const u32 line, cstr fmt, Args&&... args) {
            if (m_priority > LoggingPriority::Info)
                return;
            str message;
            std::vformat_to(
                std::back_inserter(message),
                fmt,
                std::make_format_args(std::forward<Args>(args)...));
            constexpr u8 priority_index = static_cast<u8>(LoggingPriority::Info);
            styled_log("Info", message.c_str(), m_style[priority_index][0], m_style[priority_index][1], m_style[priority_index][2], file, line);
        }

        template <typename... Args>
        void Warn(const cstr file, const u32 line, cstr fmt, Args&&... args) {
            if (m_priority > LoggingPriority::Warn)
                return;
            str message;
            std::vformat_to(
                std::back_inserter(message),
                fmt,
                std::make_format_args(std::forward<Args>(args)...));
            constexpr u8 priority_index = static_cast<u8>(LoggingPriority::Warn);
            styled_log("Warn", message.c_str(), m_style[priority_index][0], m_style[priority_index][1], m_style[priority_index][2], file, line);
        }

        template <typename... Args>
        void Error(const cstr file, const u32 line, cstr fmt, Args&&... args) {
            if (m_priority > LoggingPriority::Error)
                return;
            str message;
            std::vformat_to(
                std::back_inserter(message),
                fmt,
                std::make_format_args(std::forward<Args>(args)...));
            constexpr u8 priority_index = static_cast<u8>(LoggingPriority::Error);
            styled_log("Error", message.c_str(), m_style[priority_index][0], m_style[priority_index][1], m_style[priority_index][2], file, line);
        }

        template <typename... Args>
        void Fatal(const cstr file, const u32 line, cstr fmt, Args&&... args) {
            if (m_priority > LoggingPriority::Fatal)
                return;
            str message;
            std::vformat_to(
                std::back_inserter(message),
                fmt,
                std::make_format_args(std::forward<Args>(args)...));
            constexpr u8 priority_index = static_cast<u8>(LoggingPriority::Fatal);
            styled_log("Fatal", message.c_str(), m_style[priority_index][0], m_style[priority_index][1], m_style[priority_index][2], file, line);
        }

    private:
        LoggingSystem()
            : m_file_output{true},
#if defined(NK_RELEASE)
              m_priority{LoggingPriority::Info},
#else
              m_priority{LoggingPriority::Debug}
#endif
        {
        }

#if defined(NK_PLATFORM_WINDOWS)
        void activate_virtual_terminal();
#endif
        cstr colorize(i8 font, i8 background = -1, i8 style = -1);

        LoggingPriority m_priority = LoggingPriority::Info;
        bool m_file_output = true;
        std::mutex m_log_mutex;

        static constexpr const i8 m_style[static_cast<u8>(LoggingPriority::Fatal) + 1][3] = {
            {2, -1, -1},
            {6, -1, -1},
            {-1, -1, -1},
            {3, -1, 1},
            {1, -1, 1},
            {-1, 1, 1},
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
