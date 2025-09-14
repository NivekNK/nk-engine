#pragma once

// TODO: Temporal include
#include "glm/ext/matrix_float4x4.hpp"

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

        virtual ~App() {}

    protected:
        App(ApplicationConfig config)
            : initial_config{config} {}

        virtual bool update(f64 delta_time) { return true; }
        virtual bool render(f64 delta_time) { return true; }
        virtual void on_resized(u32 width, u32 height) {}
        
    private:
        static App* create(mem::Allocator* allocator);
        static void destroy(mem::Allocator* allocator, nk::App* app);

        friend class Engine;
    };
}
