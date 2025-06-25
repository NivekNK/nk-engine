#include "nkpch.h"

#include "core/clock.h"

#include "platform/platform.h"

namespace nk {
    Clock::Clock()
        : m_platform{nullptr},
          m_start_time{0},
          m_elapsed{0} {}

    Clock& Clock::init(Platform* platform) {
        m_platform = platform;
        return *this;
    }

    Clock& Clock::start() {
        if (!m_platform) {
            ErrorLog("nk::Clock::start No window associated with nk::Clock.");
            return *this;
        }

        m_start_time = m_platform->get_absolute_time();
        m_elapsed = 0;
        return *this;
    }

    Clock& Clock::update() {
        if (!m_platform || m_start_time == 0)
            return *this;

        m_elapsed = m_platform->get_absolute_time() - m_start_time;
        return *this;
    }

    Clock& Clock::stop() {
        m_start_time = 0;
        return *this;
    }
}
