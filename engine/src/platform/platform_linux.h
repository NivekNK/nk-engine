#pragma once

#include "platform/platform.h"

#include <chrono>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

namespace nk {
    class PlatformLinux : public Platform {
    public:
        PlatformLinux(const ApplicationConfig& config);
        ~PlatformLinux() override;

        bool pump_messages() override;
        f64 get_absolute_time() override;
        void sleep(u64 ms) override;
        PlatformBackend backend() const override { return PlatformBackend::Xcb; }

        xcb_connection_t* get_connection() const { return m_connection; }
        xcb_window_t get_window() const { return m_window; }

    private:
        xcb_connection_t* m_connection;
        xcb_screen_t* m_screen;
        xcb_key_symbols_t* m_key_symbols;
        xcb_window_t m_window;
        xcb_atom_t m_wm_protocols;
        xcb_atom_t m_wm_delete_window;
        std::chrono::steady_clock::time_point m_start_time;
    };
}
