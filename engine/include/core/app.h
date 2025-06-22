#pragma once

namespace nk {
    class Platform;
    namespace mem { class Allocator; }

    struct ApplicationConfig {
        str name;
        i16 start_pos_x;
        i16 start_pos_y;
        u32 start_width;
        u32 start_height;
    };

    class App {
    public:
        virtual ~App();

    protected:
        App(const ApplicationConfig& config);

        virtual bool update(f64 delta_time) { return true; }
        virtual bool render(f64 delta_time) { return true; }

    private:
        static void create(mem::Allocator* allocator);
        static void destroy(mem::Allocator* allocator);

        void run();

        mem::Allocator* m_allocator;
        Platform* m_platform;

        f64 m_last_time;

        static App* s_instance;
        friend class Application;
    };
}
