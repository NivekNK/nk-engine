#pragma once

#include "nk/clock.h"

namespace nk {
    class Allocator;
    class Window;

    struct ApplicationConfig {
        str name;
        i16 start_pos_x;
        i16 start_pos_y;
        u16 start_width;
        u16 start_height;
    };

    class App {
    public:
        virtual ~App();

        void run();

        static void exit();
        static App& get() { return *s_instance; }

    protected:
        App(const ApplicationConfig& config);

    private:
        Allocator* m_allocator;
        Window* m_window;

        Clock m_clock;
        f64 m_last_time;

        static App* s_instance;
    };
}
