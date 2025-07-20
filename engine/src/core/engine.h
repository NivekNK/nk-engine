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

        static void init() { get().init_impl(); }

        static void shutdown() { get().shutdown_impl(); }

        static void run() { get().run_impl(); }

        static void exit() { get().exit_impl(); }

        static Engine& get() {
            static Engine instance;
            return instance;
        }

    private:
        Engine() = default;

        void init_impl();
        void shutdown_impl();
        void run_impl();
        void exit_impl();

        bool update(f64 delta_time);
        bool render(f64 delta_time);
        bool resize(u32 width, u32 height);

        mem::Allocator* m_allocator;
        App* m_app;
        Platform* m_platform;
        Renderer* m_renderer;

        Clock m_clock;
        f64 m_last_time;

        friend bool on_resized(SystemEventCode, void*, void*, EventContext);
    };
}
