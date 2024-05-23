#pragma once

namespace nk {
    class Window;

    class Clock {
    public:
        Clock();
        ~Clock() = default;

        Clock& init(Window* window);

        Clock& start();
        Clock& update();
        Clock& stop();

        f64 elapsed() const { return m_elapsed; }

    private:
        Window* m_window;

        f64 m_start_time = 0;
        f64 m_elapsed = 0;
    };
}
