#include "nkpch.h"

#include "nk/clock.h"

#include "nk/window.h"

namespace nk {
    Clock::Clock()
        : m_window{nullptr},
          m_start_time{0},
          m_elapsed{0} {
    }

    Clock& Clock::init(Window* window) {
        m_window = window;
        return *this;
    }

    Clock& Clock::start() {
        if (!m_window) {
            ErrorLog("No window associated with nk::Clock.");
            return *this;
        }

        m_start_time = m_window->get_absolute_time();
        m_elapsed = 0;
        return *this;
    }

    Clock& Clock::update() {
        if (!m_window || m_start_time == 0)
            return *this;

        m_elapsed = m_window->get_absolute_time() - m_start_time;
        return *this;
    }

    Clock& Clock::stop() {
        m_start_time = 0;
        return *this;
    }
}
