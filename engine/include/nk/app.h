#pragma once

namespace nk {
    class Allocator;

    struct ApplicationConfig {
        str name;
        i16 start_pos_x;
        i16 start_pos_y;
        u16 start_width;
        u16 start_height;
    };

    class App {
    public:
        virtual ~App() = default;

        void run();

        static void exit();
        static App& get() { return *s_instance; }

    protected:
        App(const ApplicationConfig& config);

    private:
        static App* s_instance;
    };
}
