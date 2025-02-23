#include "nkpch.h"

namespace nk {
    int entry_point(int argc, char** argv) {
        nk::LoggingSystem::init();

        TraceLog("nk::entry_point called.");
        DebugLog("nk::entry_point called.");

        nk::LoggingSystem::shutdown();

        return EXIT_SUCCESS;
    }
}
