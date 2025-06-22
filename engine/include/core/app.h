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
        const ApplicationConfig initial_config;

        virtual ~App();

    protected:
        App(ApplicationConfig config);

        virtual bool update(f64 delta_time) { return true; }
        virtual bool render(f64 delta_time) { return true; }
        
    private:
        static App* create(mem::Allocator* allocator);
        static void destroy(mem::Allocator* allocator, nk::App* app);

        friend class Engine;
    };
}
