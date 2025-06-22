#pragma once

#include "core/app.h"
#include "systems/event_system.h"

namespace nk {
    class Application {
    public:
        Application() = delete;
        ~Application() = delete;
        
        static void create(mem::Allocator* allocator);
        static void destroy(mem::Allocator* allocator);

        static void run() { App::s_instance->run(); }

        static bool on_event(SystemEventCode code, void* sender, void* listener, EventContext context);
        static bool on_key(SystemEventCode code, void* sender, void* listener, EventContext context);
    };
}
