#include "nkpch.h"

#include "nk/entry_point.h"

#include "system/memory_system.h"
#include "memory/malloc_allocator.h"

namespace nk {
    i32 entry_point(i32 argc, char** argv) {
        NK_MEMORY_SYSTEM_INIT();

        {
            LoggingSystem::init();
            InfoLog("LoggingSystem initialized.");

            {
                MallocAllocator system_allocator;
                system_allocator.allocator_init("System", MemoryType::System);
                auto app = app_create(&system_allocator);

                NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT();

                app->run();

                NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT();

                app_destroy(&system_allocator, app);
            }

            NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT();

            InfoLog("LoggingSystem shutdown.");
            LoggingSystem::shutdown();
        }

        NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT();
        NK_MEMORY_SYSTEM_SHUTDOWN();

        return EXIT_SUCCESS;
    }
}
