#pragma once

namespace nk {
    class Allocator;
    class Window;

    struct RenderPacket {
        f64 delta_time;
    };

    class Renderer {
    public:
        virtual ~Renderer() = default;

        virtual bool draw_frame(const RenderPacket& packet) = 0;

    private:
        static Renderer* create(Allocator* allocator, Window& window, str application_name);
        static void free(Allocator* allocator, Renderer* renderer);

        friend class App;
    };
}