#pragma once

#include "platform/platform.h"

#include <chrono>
#include <wayland-client.h>

struct xdg_surface;
struct xdg_toplevel;
struct xdg_wm_base;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace nk {
    class PlatformWayland final : public Platform {
    public:
        PlatformWayland(const ApplicationConfig& config);
        ~PlatformWayland() override;

        bool pump_messages() override;
        f64 get_absolute_time() override;
        void sleep(u64 ms) override;
        PlatformBackend backend() const override { return PlatformBackend::Wayland; }

        wl_display* get_display() const { return m_display; }
        wl_surface* get_surface() const { return m_surface; }

    private:
        static void registry_global(void* data, wl_registry* registry, u32 name, const char* interface, u32 version);
        static void registry_global_remove(void* data, wl_registry* registry, u32 name);

        static void wm_base_ping(void* data, xdg_wm_base* wm_base, u32 serial);
        static void xdg_surface_configure(void* data, xdg_surface* surface, u32 serial);
        static void toplevel_configure(void* data, xdg_toplevel* toplevel, i32 width, i32 height, wl_array* states);
        static void toplevel_close(void* data, xdg_toplevel* toplevel);

        static void seat_capabilities(void* data, wl_seat* seat, u32 capabilities);
        static void seat_name(void* data, wl_seat* seat, const char* name);

        static void keyboard_keymap(void* data, wl_keyboard* keyboard, u32 format, i32 fd, u32 size);
        static void keyboard_enter(void* data, wl_keyboard* keyboard, u32 serial, wl_surface* surface, wl_array* keys);
        static void keyboard_leave(void* data, wl_keyboard* keyboard, u32 serial, wl_surface* surface);
        static void keyboard_key(void* data, wl_keyboard* keyboard, u32 serial, u32 time, u32 key, u32 state);
        static void keyboard_modifiers(
            void* data,
            wl_keyboard* keyboard,
            u32 serial,
            u32 mods_depressed,
            u32 mods_latched,
            u32 mods_locked,
            u32 group);
        static void keyboard_repeat_info(void* data, wl_keyboard* keyboard, i32 rate, i32 delay);

        static void pointer_enter(
            void* data,
            wl_pointer* pointer,
            u32 serial,
            wl_surface* surface,
            wl_fixed_t surface_x,
            wl_fixed_t surface_y);
        static void pointer_leave(void* data, wl_pointer* pointer, u32 serial, wl_surface* surface);
        static void pointer_motion(void* data, wl_pointer* pointer, u32 time, wl_fixed_t surface_x, wl_fixed_t surface_y);
        static void pointer_button(void* data, wl_pointer* pointer, u32 serial, u32 time, u32 button, u32 state);
        static void pointer_axis(void* data, wl_pointer* pointer, u32 time, u32 axis, wl_fixed_t value);

        void apply_pending_configure();
        void release_keyboard();
        void release_pointer();

        wl_display* m_display;
        wl_registry* m_registry;
        wl_compositor* m_compositor;
        wl_surface* m_surface;
        xdg_wm_base* m_wm_base;
        xdg_surface* m_xdg_surface;
        xdg_toplevel* m_toplevel;
        wl_seat* m_seat;
        wl_keyboard* m_keyboard;
        wl_pointer* m_pointer;

        xkb_context* m_xkb_context;
        xkb_keymap* m_xkb_keymap;
        xkb_state* m_xkb_state;

        u32 m_pending_width;
        u32 m_pending_height;
        bool m_configured;
        bool m_ready_for_events;
        std::chrono::steady_clock::time_point m_start_time;
    };
}
