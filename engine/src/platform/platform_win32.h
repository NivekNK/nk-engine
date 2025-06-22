#pragma once

#include "platform/platform.h"

namespace nk {
    class PlatformWin32 : public Platform {
    public:
        PlatformWin32(const ApplicationConfig& config);
        virtual ~PlatformWin32() override;

        virtual bool pump_messages() override;
        virtual f64 get_absolute_time() override;
        virtual void sleep(u64 ms) override;

    private:
        HINSTANCE m_hinstance;
        HWND m_hwnd;

        f64 m_clock_frequency;
        LARGE_INTEGER m_start_time;
    };
}
