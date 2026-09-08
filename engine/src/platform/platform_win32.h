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
        PlatformBackend backend() const override { return PlatformBackend::Win32; }

        HINSTANCE get_hinstance() { return m_hinstance; }
        HWND get_hwnd() { return m_hwnd; }

    private:
        HINSTANCE m_hinstance = nullptr;
        HWND m_hwnd = nullptr;
        wchar_t m_pending_high_surrogate = 0;

        f64 m_clock_frequency;
        LARGE_INTEGER m_start_time;

        friend LRESULT CALLBACK win32_process_message(
            HWND hwnd, u32 msg, WPARAM wparam, LPARAM lparam);
    };
}
