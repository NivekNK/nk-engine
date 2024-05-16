#include "nkpch.h"

#include "system/logging_system.h"

namespace nk {
#if defined(NK_PLATFORM_WINDOWS)
    void activate_virtual_terminal() {
        HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode;
        GetConsoleMode(handleOut, &consoleMode);
        consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(handleOut, consoleMode);
    }
#endif

    LoggingSystem::LoggingSystem()
        : m_show_path{true},
          m_short_path{true},
#if defined(NK_RELEASE)
          m_priority{LoggingPriority::Info},
#else
          m_priority{LoggingPriority::Debug},
#endif
          m_file_output{true} {
        memcpy(m_style, s_default_style, (static_cast<u8>(LoggingPriority::Fatal) + 1) * sizeof(LoggingColor));
#if defined(NK_PLATFORM_WINDOWS)
        activate_virtual_terminal();
#endif
    }

    void LoggingSystem::formatted_log(str file, const u32 line, const LoggingColor& color, cstr message, cstr log_name) {
        std::scoped_lock lock(m_log_mutex);

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm time_info;
        localtime_s(&time_info, &time_t_now);

        if (m_show_path && m_short_path) {
            const u64 pos = file.find("nk-engine");
            if (pos != std::string::npos) {
                file.erase(0, pos + 10);
            }
        }

        str formatted_message;
        if (log_name == nullptr) {
            if (m_show_path) {
                formatted_message = std::format(
                    "[{:02}:{:02}:{:02}] : {} ({}:{})",
                    time_info.tm_hour,
                    time_info.tm_min,
                    time_info.tm_sec,
                    message,
                    file,
                    line);
            }
            else {
                formatted_message = std::format(
                    "[{:02}:{:02}:{:02}] : {}",
                    time_info.tm_hour,
                    time_info.tm_min,
                    time_info.tm_sec,
                    message);
            }
        }
        else {
            if (m_show_path) {
                formatted_message = std::format(
                    "[{:02}:{:02}:{:02}] {}: {} ({}:{})",
                    time_info.tm_hour,
                    time_info.tm_min,
                    time_info.tm_sec,
                    log_name,
                    message,
                    file,
                    line);
            } else {
                formatted_message = std::format(
                    "[{:02}:{:02}:{:02}] {}: {}",
                    time_info.tm_hour,
                    time_info.tm_min,
                    time_info.tm_sec,
                    log_name,
                    message);
            }
        }

        str color_format;
        if (color.bg) {
            color_format = std::format("\033[38;2;{};{};{};48;2;{};{};{}m",
                                       color.fg.r, color.fg.g, color.fg.b,
                                       color.bg->r, color.bg->g, color.bg->b);
        }
        else {
            color_format = std::format("\033[38;2;{};{};{}m", color.fg.r, color.fg.g, color.fg.b);
        }

        std::cout << color_format
                  << formatted_message
                  << "\033[0m"
                  << std::endl;

        if (m_file_output) {
            // TODO: add file system management.
        }
    }
}
