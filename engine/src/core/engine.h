#pragma once

#include "systems/event_system.h"
#include "core/clock.h"

namespace nk {
    namespace mem { class Allocator; }
    class App;
    class Platform;
    class Renderer;

    class Engine {
    public:
        ~Engine() = default;

        static bool init() { return get().init_impl(); }

        static void shutdown() { get().shutdown_impl(); }

        static void run() { get().run_impl(); }

        static void exit() { get().exit_impl(); }

        static Engine& get() {
            static Engine instance;
            return instance;
        }

    private:
        Engine() = default;

        bool init_impl();
        void shutdown_impl();
        void run_impl();
        void exit_impl();

        bool update(f64 delta_time);
        bool render(f64 delta_time);
        bool resize(u32 width, u32 height);

        mem::Allocator* m_allocator = nullptr;
        App* m_app = nullptr;
        Platform* m_platform = nullptr;
        Renderer* m_renderer = nullptr;
        bool m_initialized = false;

        Clock m_clock;
        f64 m_last_time = 0.0;

        friend bool on_resized(SystemEventCode, void*, void*, EventContext);
        friend bool on_key(SystemEventCode, void*, void*, EventContext);
    };
}
