#pragma once

namespace nk {
    class Platform;

    class Clock {
    public:
        Clock();
        ~Clock() = default;

        Clock& init(Platform* platform);

        Clock& start();
        Clock& update();
        Clock& stop();

        f64 elapsed() const { return m_elapsed; }

    private:
        Platform* m_platform;

        f64 m_start_time;
        f64 m_elapsed;
    };
}
