#pragma once

namespace nk {
    namespace mem { class Allocator; }
    class Platform;

    struct RenderPacket {
        f64 delta_time;
    };

    class Renderer {
    public:
        virtual ~Renderer() = default;

        static Renderer* create(mem::Allocator* allocator, Platform* platform, str application_name);
        static void destroy(mem::Allocator* allocator, Renderer* renderer);

        bool draw_frame(const RenderPacket& packet);

        virtual void on_resized(u32 with, u32 height) = 0;
    
    protected:
        virtual void init() = 0;
        virtual void shutdown() = 0;
        virtual bool begin_frame(f64 delta_time) = 0;
        virtual bool end_frame(f64 delta_time) = 0;

        str m_application_name;
        Platform* m_platform;

        mem::Allocator* m_allocator;

        u64 m_frame_number;

    private:
        bool end_frame_impl(f64 delta_time);
    };
}
