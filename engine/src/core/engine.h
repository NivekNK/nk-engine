#pragma once

#include "systems/event_system.h"
#include "platform/platform.h"

namespace nk {
    namespace mem { class Allocator; }
    class App;

    class Engine {
    public:
        ~Engine() = default;

        static void init() {
            Engine& instance = get();
            instance.init_impl();
        }

        static void shutdown() {
            Engine& instance = get();
            instance.shutdown_impl();
        }

        static void run() {
            Engine& instance = get();
            instance.run_impl();
        }

        static void exit() {
            Engine& instance = get();
            instance.m_platform->close();
        }

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

        f64 m_last_time;
    };
}
