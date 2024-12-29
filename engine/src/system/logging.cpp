#include "nkpch.h"

#include "system/logging.h"

#include <spdlog/sinks/ansicolor_sink.h>

namespace nk {
    LoggingSystem& LoggingSystem::init() {
        LoggingSystem& instance = get();

        spdlog::set_pattern("%^[%T] %n %l: %v%$");

        spdlog::sinks::ansicolor_stdout_sink_mt* m_client_sink;

        return instance;
    }

    void LoggingSystem::shutdown() {}

    spdlog::logger* LoggingSystem::get_engine_logger() {
        if (!m_engine_logger) {
            return nullptr;
        }

        return m_engine_logger;
    }
    
    spdlog::logger* LoggingSystem::get_client_logger() {
        if (!m_client_logger) {
            return nullptr;
        }

        return m_client_logger;
    }
}
