#include "nkpch.h"

#include "system/logging_system.h"

namespace nk {
    void LoggingSystem::named_log(
        const cstr log_name,
        const cstr message,
        i8 font, i8 background, i8 style,
        str file,
        const u32 line) {
        std::scoped_lock lock(m_log_mutex);

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm time_info;
        localtime_s(&time_info, &time_t_now);

        if (m_short_path) {
            const u64 pos = file.find("nk-engine");
            if (pos != std::string::npos) {
                file.erase(0, pos + 10);
            }
        }

        str formatted_message = std::format(
            "[{:02}:{:02}:{:02}] {}: {} ({}:{})",
            time_info.tm_hour,
            time_info.tm_min,
            time_info.tm_sec,
            log_name,
            message,
            file,
            line);

        std::cout << colorize(font, background, style)
                  << formatted_message
                  << "\033[0m"
                  << std::endl;

        if (m_file_output) {
            // TODO: add file system management.
        }
    }

#if defined(NK_PLATFORM_WINDOWS)
    void LoggingSystem::activate_virtual_terminal() {
        HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode;
        GetConsoleMode(handleOut, &consoleMode);
        consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(handleOut, consoleMode);
    }
#endif

    cstr LoggingSystem::colorize(i8 font, i8 background, i8 style) {
        static char code[20];

        if (font >= 0)
            font += 30;
        else
            font = 0;

        if (background >= 0)
            background += 40;
        else
            background = 0;

        if (background > 0 && style > 0) {
            sprintf(code, "\033[%d;%d;%dm", font, background, style);
        } else if (background > 0) {
            sprintf(code, "\033[%d;%dm", font, background);
        } else if (style > 0) {
            sprintf(code, "\033[%d;%dm", style, font);
        } else {
            sprintf(code, "\033[%dm", font);
        }

        return code;
    }

    void LoggingSystem::styled_log(i8 font, i8 background, i8 style, cstr formatted_message) {
        std::scoped_lock lock(m_log_mutex);

        std::cout << colorize(font, background, style)
                  << formatted_message
                  << "\033[0m"
                  << std::endl;

        if (m_file_output) {
            // TODO: add file system management.
        }
    }
}
