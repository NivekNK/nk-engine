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
struct xkb_compose_table;
struct xkb_compose_state;
struct wp_viewporter;
struct wp_viewport;
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;
struct zwp_text_input_manager_v3;
struct zwp_text_input_v3;

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
        static void fractional_preferred_scale(void* data, wp_fractional_scale_v1*, u32 scale);
        static void text_input_enter(void* data, zwp_text_input_v3*, wl_surface*);
        static void text_input_leave(void* data, zwp_text_input_v3*, wl_surface*);
        static void text_input_preedit(void* data, zwp_text_input_v3*, const char*, i32, i32);
        static void text_input_commit(void* data, zwp_text_input_v3*, const char*);
        static void text_input_delete(void* data, zwp_text_input_v3*, u32, u32);
        static void text_input_done(void* data, zwp_text_input_v3*, u32);

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
        void sync_text_input();
        void emit_text(u32 wayland_key);
        void process_repeat();
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
        wp_viewporter* m_viewporter;
        wp_viewport* m_viewport;
        wp_fractional_scale_manager_v1* m_fractional_scale_manager;
        wp_fractional_scale_v1* m_fractional_scale;
        zwp_text_input_manager_v3* m_text_input_manager;
        zwp_text_input_v3* m_text_input;

        xkb_context* m_xkb_context;
        xkb_keymap* m_xkb_keymap;
        xkb_state* m_xkb_state;
        xkb_compose_table* m_compose_table;
        xkb_compose_state* m_compose_state;

        u32 m_pending_width;
        u32 m_pending_height;
        bool m_configured;
        bool m_ready_for_events;
        bool m_text_input_focused = false;
        bool m_text_input_active = false;
        u32 m_repeat_key = numeric::invalid_id;
        i32 m_repeat_rate = 0, m_repeat_delay = 0;
        std::chrono::steady_clock::time_point m_next_repeat{};
        std::chrono::steady_clock::time_point m_start_time;
    };
}
