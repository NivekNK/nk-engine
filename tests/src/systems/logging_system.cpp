#include <gtest/gtest.h>

#include "systems/logging_system.h"

TEST(LoggingSystem, LoggingSystemInit) {
    nk::LoggingSystem::init();
    nk::LoggingSystem::log(nk::LoggingLevel::Trace, __FILE__, 6, "MESSAGE");
    nk::LoggingSystem::log(nk::LoggingLevel::Debug, __FILE__, 6, "MESSAGE");
    nk::LoggingSystem::log(nk::LoggingLevel::Info, __FILE__, 6, "MESSAGE");
    nk::LoggingSystem::log(nk::LoggingLevel::Warning, __FILE__, 6, "MESSAGE");
    nk::LoggingSystem::log(nk::LoggingLevel::Error, __FILE__, 6, "MESSAGE");
    nk::LoggingSystem::log(nk::LoggingLevel::Fatal, __FILE__, 6, "MESSAGE");
    nk::LoggingSystem::log(nk::LoggingLevel::None, __FILE__, 6, "MESSAGE");
    nk::LoggingSystem::shutdown();
}
