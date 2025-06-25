#include "nkpch.h"

#include "core/entry_point.h"

#include "systems/memory_system.h"
#include "systems/event_system.h"
#include "systems/input_system.h"
#include "core/engine.h"

namespace nk {
    int entry_point(int argc, char** argv) {
        NK_MEMORY_SYSTEM_INIT();

        {
            LoggingSystem::init();
            EventSystem::init();
            InputSystem::init();

            Engine::init();
            NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();
            Engine::run();
            Engine::shutdown();
            NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();

            InputSystem::shutdown();
            EventSystem::shutdown();
            LoggingSystem::shutdown();
        }

        NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();
        NK_MEMORY_SYSTEM_SHUTDOWN();

        return EXIT_SUCCESS;
    }
}
