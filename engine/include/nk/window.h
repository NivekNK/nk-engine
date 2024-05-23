#pragma once

namespace nk {
    struct ApplicationConfig;
    class Allocator;

    class Window {
    public:
        virtual ~Window() = default;

        virtual bool pump_messages() = 0;
        virtual f64 get_absolute_time() = 0;
        virtual void sleep(u64 ms) = 0;

        bool update(f64 delta_time) { return true; }
        bool render(f64 delta_time) { return true; }

        bool is_suspended() const { return m_suspended; }
        bool is_running() const { return m_running; }
        u16 width() const { return m_width; }
        u16 height() const { return m_height; }

    protected:
        Window(const ApplicationConfig& config);

        bool m_suspended;
        bool m_running;

        i16 m_pos_x;
        i16 m_pos_y;
        u16 m_width;
        u16 m_height;
    };

    Window* window_create(Allocator* allocator, const ApplicationConfig& config);
    void window_destroy(Allocator* allocator, Window* window);
}
