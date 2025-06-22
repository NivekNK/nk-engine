#include "nkpch.h"

#include "core/entry_point.h"

#include "systems/memory_system.h"
#include "memory/malloc_allocator.h"
#include "systems/event_system.h"
#include "systems/input_system.h"
#include "core/application.h"

namespace nk {
    int entry_point(int argc, char** argv) {
        NK_MEMORY_SYSTEM_INIT();

        {
            mem::MallocAllocator system_allocator;
            system_allocator.allocator_init(mem::MallocAllocator, "System", MemoryType::System);

            LoggingSystem::init();
            EventSystem::init();
            InputSystem::init();

            Application::create(&system_allocator);
            NK_MEMORY_SYSTEM_LOG_REPORT();
            Application::run();
            Application::destroy(&system_allocator);
            NK_MEMORY_SYSTEM_LOG_REPORT();

            InputSystem::shutdown();
            EventSystem::shutdown();
            LoggingSystem::shutdown();
        }

        NK_MEMORY_SYSTEM_LOG_REPORT();
        NK_MEMORY_SYSTEM_SHUTDOWN();

        return EXIT_SUCCESS;
    }
}
