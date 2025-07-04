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

        static void exit();

        static Engine& get() {
            static Engine instance;
            return instance;
        }

    private:
        Engine() = default;

        void init_impl();
        void shutdown_impl();
        void run_impl();

        bool update(f64 delta_time);
        bool render(f64 delta_time);

        mem::Allocator* m_allocator;
        App* m_app;
        Platform* m_platform;
        Renderer* m_renderer;

        Clock m_clock;
        f64 m_last_time;
    };
}
