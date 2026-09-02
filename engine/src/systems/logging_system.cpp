#include "nkpch.h"

#include "systems/logging_system.h"

namespace nk {
    LoggingSystem& LoggingSystem::init(const LoggingSystemConfig& config) {
        LoggingSystem& instance = get();

        for (u8 i = 0; i < static_cast<u8>(LoggingLevel::Off); i++) {
            const LoggingColor color = config.style[i];
            if (color.bg) {
                format_to(
                    instance.m_style[i],
                    "\033[38;2;{};{};{};48;2;{};{};{}m",
                    color.fg.r,
                    color.fg.g,
                    color.fg.b,
                    color.bg->r,
                    color.bg->g,
                    color.bg->b);
            } else {
                format_to(
                    instance.m_style[i],
                    "\033[38;2;{};{};{}m",
                    color.fg.r,
                    color.fg.g,
                    color.fg.b);
            }
        }

        instance.m_priority = config.priority;
        instance.m_show_file = config.show_file;
        instance.m_show_time = config.show_time;
        instance.m_file_output = config.file_output;

        TraceLog("nk::LoggingSystem Inititalized.");

        return instance;
    }

    void LoggingSystem::shutdown() {
        TraceLog("nk::LoggingSystem Shutdown.");
    }

    strview get_project_path() noexcept {
#if defined(NK_PROJECT_PATH)
        return strview{NK_PROJECT_PATH};
#else
        return {};
#endif
    }

    void LoggingSystem::log(
        const LoggingLevel level,
        const strview file,
        const u32 line,
        const strview message) noexcept {
        if (level == LoggingLevel::Off || level < get().m_priority)
            return;

        const u8 index = static_cast<u8>(level);
        auto& instance = get();

        // Add color
        const strbuf<64>& color = instance.m_style[index];
        os::write(color.data(), color.length());

        strbuf<4096> buffer;

        if (instance.m_show_time) {
            const auto now = std::chrono::system_clock::now();
            const auto time_value = std::chrono::system_clock::to_time_t(now);
            std::tm local_time{};
#if defined(NK_PLATFORM_WINDOWS)
            const bool has_time = ::localtime_s(&local_time, &time_value) == 0;
#else
            const bool has_time = ::localtime_r(&time_value, &local_time) != nullptr;
#endif
            if (has_time) {
                strbuf<16> timestamp;
                format_to(
                    timestamp,
                    "{:02}:{:02}:{:02}",
                    local_time.tm_hour,
                    local_time.tm_min,
                    local_time.tm_sec);
                buffer.append(timestamp.view());
            }
        }

        if (level != LoggingLevel::None) {
            const auto& level_string = instance.logging_level[index];
            buffer.append(strview{level_string.value, level_string.size});
        } else {
            buffer.append(" ");
        }

        buffer.append(message);

        if (instance.m_show_file) {
            const strview project_path = get_project_path();
            const u64 position = project_path.empty()
                ? strview::npos
                : file.find(project_path);
            const strview displayed_file = position == strview::npos
                ? file
                : file.substr(position + project_path.length() + 1);
            strbuf<1024> location;
            format_to(location, " ({}:{})", displayed_file, line);
            buffer.append(location.view());
        }

        buffer.mark_truncated();

        os::write(buffer.data(), buffer.length());
        os::write("\033[0m\n", 5);
        os::flush();
    }
}
