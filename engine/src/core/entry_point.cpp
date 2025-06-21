#include "nkpch.h"

#include "systems/memory_system.h"

namespace nk {
    int entry_point(int argc, char** argv) {
        NK_MEMORY_SYSTEM_INIT();

        nk::LoggingSystem::init();

        

        nk::LoggingSystem::shutdown();

        NK_MEMORY_SYSTEM_SHUTDOWN();

        return EXIT_SUCCESS;
    }
}
