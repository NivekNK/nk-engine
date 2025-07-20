#pragma once

namespace nk {
    struct ApplicationConfig;

    namespace mem {
        class Allocator;
    }

    class Platform {
    public:
        virtual ~Platform() = default;

        virtual bool pump_messages() = 0;
        virtual f64 get_absolute_time() = 0;
        virtual void sleep(u64 ms) = 0;

        bool suspended() const { return m_suspended; }
        bool running() const { return m_running; }
        u32 width() const { return m_width; }
        u32 height() const { return m_height; }

        void close() { m_running = false; }
        void on_resized(u32 width, u32 height) {
            m_width = width;
            m_height = height;
        }
        void set_suspended(bool suspended) { m_suspended = suspended; }

        static Platform* create(mem::Allocator* allocator, const ApplicationConfig& config);
        static void destroy(mem::Allocator* allocator, Platform* platform);

    protected:
        Platform(const ApplicationConfig& config);

        bool m_suspended;
        bool m_running;

        i16 m_pos_x;
        i16 m_pos_y;
        u32 m_width;
        u32 m_height;
    };
}
