#include "nkpch.h"

#include "systems/logging_system.h"

namespace nk {
    LoggingSystem& LoggingSystem::init(const LoggingSystemConfig& config) {
        LoggingSystem& instance = get();

        for (u8 i = 0; i < static_cast<u8>(LoggingLevel::Off); i++) {
            LoggingColor color = config.style[i];
            if (color.bg) {
                instance.style[i] = std::format("\033[38;2;{};{};{};48;2;{};{};{}m",
                                                color.fg.r, color.fg.g, color.fg.b,
                                                color.bg->r, color.bg->g, color.bg->b);
            } else {
                instance.style[i] = std::format("\033[38;2;{};{};{}m",
                                                color.fg.r, color.fg.g, color.fg.b);
            }
        }

        instance.priority = config.priority;
        instance.show_file = config.show_file;
        instance.show_time = config.show_time;
        instance.file_output = config.file_output;

        return instance;
    }

    void LoggingSystem::shutdown() {}

    // TODO: Move to a more generalized place
    std::string get_project_path() {
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        return NK_PROJECT_PATH;
#else
        return std::filesystem::current_path().string();
#endif
    }

    void LoggingSystem::log(LoggingLevel level, std::string_view file, u32 line, std::string_view message) {
        if (level == LoggingLevel::Off)
            return;

        const u8 index = static_cast<u8>(level);
        auto& instance = get();

        // Add color
        const std::string& color = instance.style[index];
        os::write(color.c_str(), color.size());

        std::string buffer;
        buffer.reserve(128);

        if (instance.show_time) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto* tm = std::localtime(&time_t);

            buffer.append(std::format("{:02}:{:02}:{:02}", tm->tm_hour, tm->tm_min, tm->tm_sec));
        }

        if (level != LoggingLevel::None) {
            const auto& level_string = instance.logging_level[index];
            buffer.append(level_string.value, level_string.size);
        } else {
            buffer.append(" ");
        }

        buffer.append(message);

        if (instance.show_file) {
            auto project_path = get_project_path();
            auto pos = file.find(project_path);
            if (pos != std::string_view::npos) {
                buffer.append(std::format(" ({}:{})", file.substr(pos + project_path.size() + 1), line));
            } else {
                buffer.append(std::format(" ({}:{})", file, line));
            }
        }

        buffer.append("\033[0m\n", 5);

        os::write(buffer.data(), buffer.length());
    }
}
