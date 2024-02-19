#include "system/logging_system.h"

int main(void) {
    nk::LoggingSystem::init();

    DebugLog("{} test", 1);

    nk::LoggingSystem::shutdown();
    return 0;
}
