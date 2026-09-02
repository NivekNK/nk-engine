#include "nkpch.h"

#include "platform/platform_wayland.h"

#include "core/app.h"
#include "systems/event_system.h"
#include "systems/input_system.h"

#include "xdg-shell-client-protocol.h"

#include <algorithm>
#include <cerrno>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

namespace {
    nk::KeyCodeFlag translate_keysym(xkb_keysym_t keysym) {
        if (keysym >= XKB_KEY_a && keysym <= XKB_KEY_z)
            return nk::KeyCode::A + static_cast<nk::KeyCodeFlag>(keysym - XKB_KEY_a);
        if (keysym >= XKB_KEY_A && keysym <= XKB_KEY_Z)
            return nk::KeyCode::A + static_cast<nk::KeyCodeFlag>(keysym - XKB_KEY_A);
        if (keysym >= XKB_KEY_0 && keysym <= XKB_KEY_9)
            return nk::KeyCode::Num0 + static_cast<nk::KeyCodeFlag>(keysym - XKB_KEY_0);
        if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F24)
            return nk::KeyCode::F1 + static_cast<nk::KeyCodeFlag>(keysym - XKB_KEY_F1);
        if (keysym >= XKB_KEY_KP_0 && keysym <= XKB_KEY_KP_9)
            return nk::KeyCode::Numpad0 + static_cast<nk::KeyCodeFlag>(keysym - XKB_KEY_KP_0);

        switch (keysym) {
            case XKB_KEY_BackSpace: return nk::KeyCode::Backspace;
            case XKB_KEY_Return:
            case XKB_KEY_KP_Enter: return nk::KeyCode::Enter;
            case XKB_KEY_Tab:
            case XKB_KEY_ISO_Left_Tab: return nk::KeyCode::Tab;
            case XKB_KEY_Shift_L: return nk::KeyCode::LShift;
            case XKB_KEY_Shift_R: return nk::KeyCode::RShift;
            case XKB_KEY_Control_L: return nk::KeyCode::LCtrl;
            case XKB_KEY_Control_R: return nk::KeyCode::RCtrl;
            case XKB_KEY_Alt_L:
            case XKB_KEY_Meta_L: return nk::KeyCode::LAlt;
            case XKB_KEY_Alt_R:
            case XKB_KEY_Meta_R:
            case XKB_KEY_ISO_Level3_Shift: return nk::KeyCode::RAlt;
            case XKB_KEY_Pause: return nk::KeyCode::Pause;
            case XKB_KEY_Caps_Lock: return nk::KeyCode::Capital;
            case XKB_KEY_Escape: return nk::KeyCode::Escape;
            case XKB_KEY_space: return nk::KeyCode::Space;
            case XKB_KEY_Page_Up: return nk::KeyCode::PageUp;
            case XKB_KEY_Page_Down: return nk::KeyCode::PageDown;
            case XKB_KEY_End: return nk::KeyCode::End;
            case XKB_KEY_Home: return nk::KeyCode::Home;
            case XKB_KEY_Left: return nk::KeyCode::Left;
            case XKB_KEY_Up: return nk::KeyCode::Up;
            case XKB_KEY_Right: return nk::KeyCode::Right;
            case XKB_KEY_Down: return nk::KeyCode::Down;
            case XKB_KEY_Print: return nk::KeyCode::PrintScreen;
            case XKB_KEY_Insert: return nk::KeyCode::Insert;
            case XKB_KEY_Delete: return nk::KeyCode::Delete;
            case XKB_KEY_Super_L: return nk::KeyCode::LSuper;
            case XKB_KEY_Super_R: return nk::KeyCode::RSuper;
            case XKB_KEY_Menu: return nk::KeyCode::Apps;
            case XKB_KEY_Num_Lock: return nk::KeyCode::NumLock;
            case XKB_KEY_Scroll_Lock: return nk::KeyCode::Scroll;
            case XKB_KEY_KP_Multiply: return nk::KeyCode::Multiply;
            case XKB_KEY_KP_Add: return nk::KeyCode::Add;
            case XKB_KEY_KP_Separator: return nk::KeyCode::Separator;
            case XKB_KEY_KP_Subtract: return nk::KeyCode::Subtract;
            case XKB_KEY_KP_Decimal:
            case XKB_KEY_KP_Delete: return nk::KeyCode::Secimal;
            case XKB_KEY_KP_Divide: return nk::KeyCode::Divide;
            case XKB_KEY_KP_Equal: return nk::KeyCode::NumpadEqual;
            case XKB_KEY_KP_Insert: return nk::KeyCode::Numpad0;
            case XKB_KEY_KP_End: return nk::KeyCode::Numpad1;
            case XKB_KEY_KP_Down: return nk::KeyCode::Numpad2;
            case XKB_KEY_KP_Next: return nk::KeyCode::Numpad3;
            case XKB_KEY_KP_Left: return nk::KeyCode::Numpad4;
            case XKB_KEY_KP_Begin: return nk::KeyCode::Numpad5;
            case XKB_KEY_KP_Right: return nk::KeyCode::Numpad6;
            case XKB_KEY_KP_Home: return nk::KeyCode::Numpad7;
            case XKB_KEY_KP_Up: return nk::KeyCode::Numpad8;
            case XKB_KEY_KP_Prior: return nk::KeyCode::Numpad9;
            case XKB_KEY_semicolon: return nk::KeyCode::Semicolon;
            case XKB_KEY_apostrophe: return nk::KeyCode::Apostrophe;
            case XKB_KEY_equal: return nk::KeyCode::Equal;
            case XKB_KEY_comma: return nk::KeyCode::Comma;
            case XKB_KEY_minus: return nk::KeyCode::Minus;
            case XKB_KEY_period: return nk::KeyCode::Period;
            case XKB_KEY_slash: return nk::KeyCode::Slash;
            case XKB_KEY_grave: return nk::KeyCode::Grave;
            case XKB_KEY_bracketleft: return nk::KeyCode::LBracket;
            case XKB_KEY_backslash: return nk::KeyCode::Backslash;
            case XKB_KEY_bracketright: return nk::KeyCode::RBracket;
            default: return 0;
        }
    }

    nk::KeyCodeFlag translate_key(xkb_keymap* keymap, xkb_state* state, nk::u32 wayland_key) {
        if (keymap == nullptr || state == nullptr)
            return 0;

        const xkb_keycode_t keycode = wayland_key + 8;
        const xkb_layout_index_t layout = xkb_state_key_get_layout(state, keycode);
        const xkb_keysym_t* keysyms = nullptr;
        const int count = layout == XKB_LAYOUT_INVALID
                              ? 0
                              : xkb_keymap_key_get_syms_by_level(keymap, keycode, layout, 0, &keysyms);
        const xkb_keysym_t keysym = count > 0 ? keysyms[0] : xkb_state_key_get_one_sym(state, keycode);
        return translate_keysym(keysym);
    }

    nk::i16 fixed_to_i16(wl_fixed_t value) {
        const nk::i32 integer = wl_fixed_to_int(value);
        return static_cast<nk::i16>(
            std::clamp(integer, static_cast<nk::i32>(INT16_MIN), static_cast<nk::i32>(INT16_MAX)));
    }
}

namespace nk {
    PlatformWayland::PlatformWayland(const ApplicationConfig& config)
        : Platform(config),
          m_display{nullptr},
          m_registry{nullptr},
          m_compositor{nullptr},
          m_surface{nullptr},
          m_wm_base{nullptr},
          m_xdg_surface{nullptr},
          m_toplevel{nullptr},
          m_seat{nullptr},
          m_keyboard{nullptr},
          m_pointer{nullptr},
          m_xkb_context{nullptr},
          m_xkb_keymap{nullptr},
          m_xkb_state{nullptr},
          m_pending_width{config.start_width},
          m_pending_height{config.start_height},
          m_configured{false},
          m_ready_for_events{false},
          m_start_time{std::chrono::steady_clock::now()} {
        m_display = wl_display_connect(nullptr);
        if (m_display == nullptr) {
            ErrorLog("Unable to connect to the Wayland compositor. Check WAYLAND_DISPLAY and XDG_RUNTIME_DIR.");
            m_running = false;
            return;
        }

        m_registry = wl_display_get_registry(m_display);
        if (m_registry == nullptr) {
            ErrorLog("Unable to obtain the Wayland registry.");
            m_running = false;
            return;
        }

        static const wl_registry_listener registry_listener = [] {
            wl_registry_listener listener{};
            listener.global = registry_global;
            listener.global_remove = registry_global_remove;
            return listener;
        }();
        wl_registry_add_listener(m_registry, &registry_listener, this);

        if (wl_display_roundtrip(m_display) < 0) {
            ErrorLog("Wayland registry discovery failed.");
            m_running = false;
            return;
        }

        if (m_compositor == nullptr || m_wm_base == nullptr) {
            ErrorLog("The Wayland compositor does not provide wl_compositor and xdg_wm_base.");
            m_running = false;
            return;
        }

        m_surface = wl_compositor_create_surface(m_compositor);
        if (m_surface == nullptr) {
            ErrorLog("Unable to create a Wayland surface.");
            m_running = false;
            return;
        }

        m_xdg_surface = xdg_wm_base_get_xdg_surface(m_wm_base, m_surface);
        if (m_xdg_surface == nullptr) {
            ErrorLog("Unable to create an xdg-shell surface.");
            m_running = false;
            return;
        }

        static const xdg_surface_listener surface_listener = [] {
            xdg_surface_listener listener{};
            listener.configure = xdg_surface_configure;
            return listener;
        }();
        xdg_surface_add_listener(m_xdg_surface, &surface_listener, this);

        m_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
        if (m_toplevel == nullptr) {
            ErrorLog("Unable to create an xdg-shell toplevel.");
            m_running = false;
            return;
        }

        static const xdg_toplevel_listener toplevel_listener = [] {
            xdg_toplevel_listener listener{};
            listener.configure = toplevel_configure;
            listener.close = toplevel_close;
            return listener;
        }();
        xdg_toplevel_add_listener(m_toplevel, &toplevel_listener, this);
        strbuf<256> title{config.name};
        title.mark_truncated();
        xdg_toplevel_set_title(m_toplevel, title.cstr());
        xdg_toplevel_set_app_id(m_toplevel, "nk-engine");

        // A role-less empty commit asks the compositor for the initial configure.
        wl_surface_commit(m_surface);
        if (wl_display_roundtrip(m_display) < 0 || !m_configured) {
            ErrorLog("The Wayland compositor did not configure the xdg-shell surface.");
            m_running = false;
            return;
        }

        m_ready_for_events = true;
        InfoLog("PlatformWayland created using native xdg-shell.");
    }

    PlatformWayland::~PlatformWayland() {
        release_keyboard();
        release_pointer();

        if (m_xkb_context != nullptr)
            xkb_context_unref(m_xkb_context);

        if (m_toplevel != nullptr)
            xdg_toplevel_destroy(m_toplevel);
        if (m_xdg_surface != nullptr)
            xdg_surface_destroy(m_xdg_surface);
        if (m_surface != nullptr)
            wl_surface_destroy(m_surface);
        if (m_seat != nullptr)
            wl_seat_destroy(m_seat);
        if (m_wm_base != nullptr)
            xdg_wm_base_destroy(m_wm_base);
        if (m_compositor != nullptr)
            wl_compositor_destroy(m_compositor);
        if (m_registry != nullptr)
            wl_registry_destroy(m_registry);

        if (m_display != nullptr) {
            wl_display_flush(m_display);
            wl_display_disconnect(m_display);
        }

        InfoLog("PlatformWayland destroyed.");
    }

    bool PlatformWayland::pump_messages() {
        if (m_display == nullptr || wl_display_get_error(m_display) != 0)
            return false;

        if (wl_display_dispatch_pending(m_display) < 0)
            return false;

        while (wl_display_prepare_read(m_display) != 0) {
            if (wl_display_dispatch_pending(m_display) < 0)
                return false;
        }

        if (wl_display_flush(m_display) < 0 && errno != EAGAIN) {
            wl_display_cancel_read(m_display);
            return false;
        }

        pollfd descriptor{
            .fd = wl_display_get_fd(m_display),
            .events = POLLIN,
            .revents = 0,
        };
        const int poll_result = poll(&descriptor, 1, 0);
        if (poll_result < 0) {
            wl_display_cancel_read(m_display);
            return errno == EINTR;
        }
        if (poll_result == 0) {
            wl_display_cancel_read(m_display);
            return true;
        }
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            wl_display_cancel_read(m_display);
            return false;
        }
        if ((descriptor.revents & POLLIN) == 0) {
            wl_display_cancel_read(m_display);
            return true;
        }
        if (wl_display_read_events(m_display) < 0)
            return false;

        return wl_display_dispatch_pending(m_display) >= 0;
    }

    f64 PlatformWayland::get_absolute_time() {
        return std::chrono::duration<f64>(std::chrono::steady_clock::now() - m_start_time).count();
    }

    void PlatformWayland::sleep(u64 ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    void PlatformWayland::registry_global(
        void* data,
        wl_registry* registry,
        u32 name,
        const char* interface,
        u32 version) {
        PlatformWayland* platform = static_cast<PlatformWayland*>(data);

        if (std::strcmp(interface, wl_compositor_interface.name) == 0 && platform->m_compositor == nullptr) {
            platform->m_compositor = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 1u)));
            return;
        }

        if (std::strcmp(interface, xdg_wm_base_interface.name) == 0 && platform->m_wm_base == nullptr) {
            platform->m_wm_base = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 1u)));

            static const xdg_wm_base_listener wm_base_listener = [] {
                xdg_wm_base_listener listener{};
                listener.ping = wm_base_ping;
                return listener;
            }();
            xdg_wm_base_add_listener(platform->m_wm_base, &wm_base_listener, platform);
            return;
        }

        if (std::strcmp(interface, wl_seat_interface.name) == 0 && platform->m_seat == nullptr) {
            platform->m_seat = static_cast<wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 4u)));

            static const wl_seat_listener seat_listener = [] {
                wl_seat_listener listener{};
                listener.capabilities = seat_capabilities;
                listener.name = seat_name;
                return listener;
            }();
            wl_seat_add_listener(platform->m_seat, &seat_listener, platform);
        }
    }

    void PlatformWayland::registry_global_remove(void*, wl_registry*, u32) {}

    void PlatformWayland::wm_base_ping(void*, xdg_wm_base* wm_base, u32 serial) {
        xdg_wm_base_pong(wm_base, serial);
    }

    void PlatformWayland::xdg_surface_configure(void* data, xdg_surface* surface, u32 serial) {
        PlatformWayland* platform = static_cast<PlatformWayland*>(data);
        xdg_surface_ack_configure(surface, serial);
        platform->m_configured = true;
        platform->apply_pending_configure();
    }

    void PlatformWayland::toplevel_configure(void* data, xdg_toplevel*, i32 width, i32 height, wl_array*) {
        PlatformWayland* platform = static_cast<PlatformWayland*>(data);
        if (width > 0)
            platform->m_pending_width = static_cast<u32>(width);
        if (height > 0)
            platform->m_pending_height = static_cast<u32>(height);
    }

    void PlatformWayland::toplevel_close(void* data, xdg_toplevel*) {
        PlatformWayland* platform = static_cast<PlatformWayland*>(data);
        if (!platform->m_ready_for_events) {
            platform->m_running = false;
            return;
        }
        EventSystem::fire_event(SystemEventCode::ApplicationQuit, nullptr, EventContext{});
    }

    void PlatformWayland::seat_capabilities(void* data, wl_seat* seat, u32 capabilities) {
        PlatformWayland* platform = static_cast<PlatformWayland*>(data);

        if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && platform->m_keyboard == nullptr) {
            platform->m_keyboard = wl_seat_get_keyboard(seat);
            static const wl_keyboard_listener keyboard_listener = [] {
                wl_keyboard_listener listener{};
                listener.keymap = keyboard_keymap;
                listener.enter = keyboard_enter;
                listener.leave = keyboard_leave;
                listener.key = keyboard_key;
                listener.modifiers = keyboard_modifiers;
                listener.repeat_info = keyboard_repeat_info;
                return listener;
            }();
            wl_keyboard_add_listener(platform->m_keyboard, &keyboard_listener, platform);
        } else if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) == 0) {
            platform->release_keyboard();
        }

        if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && platform->m_pointer == nullptr) {
            platform->m_pointer = wl_seat_get_pointer(seat);
            static const wl_pointer_listener pointer_listener = [] {
                wl_pointer_listener listener{};
                listener.enter = pointer_enter;
                listener.leave = pointer_leave;
                listener.motion = pointer_motion;
                listener.button = pointer_button;
                listener.axis = pointer_axis;
                return listener;
            }();
            wl_pointer_add_listener(platform->m_pointer, &pointer_listener, platform);
        } else if ((capabilities & WL_SEAT_CAPABILITY_POINTER) == 0) {
            platform->release_pointer();
        }
    }

    void PlatformWayland::seat_name(void*, wl_seat*, const char*) {}

    void PlatformWayland::keyboard_keymap(void* data, wl_keyboard*, u32 format, i32 fd, u32 size) {
        PlatformWayland* platform = static_cast<PlatformWayland*>(data);
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size == 0) {
            ::close(fd);
            return;
        }

        void* mapping = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapping == MAP_FAILED) {
            ErrorLog("Unable to map the Wayland XKB keymap.");
            ::close(fd);
            return;
        }

        if (platform->m_xkb_context == nullptr)
            platform->m_xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

        xkb_keymap* new_keymap = nullptr;
        xkb_state* new_state = nullptr;
        if (platform->m_xkb_context != nullptr) {
            new_keymap = xkb_keymap_new_from_string(
                platform->m_xkb_context,
                static_cast<const char*>(mapping),
                XKB_KEYMAP_FORMAT_TEXT_V1,
                XKB_KEYMAP_COMPILE_NO_FLAGS);
            if (new_keymap != nullptr)
                new_state = xkb_state_new(new_keymap);
        }

        munmap(mapping, size);
        ::close(fd);

        if (new_keymap == nullptr || new_state == nullptr) {
            if (new_state != nullptr)
                xkb_state_unref(new_state);
            if (new_keymap != nullptr)
                xkb_keymap_unref(new_keymap);
            ErrorLog("Unable to compile the Wayland XKB keymap.");
            return;
        }

        if (platform->m_xkb_state != nullptr)
            xkb_state_unref(platform->m_xkb_state);
        if (platform->m_xkb_keymap != nullptr)
            xkb_keymap_unref(platform->m_xkb_keymap);
        platform->m_xkb_keymap = new_keymap;
        platform->m_xkb_state = new_state;
    }

    void PlatformWayland::keyboard_enter(void*, wl_keyboard*, u32, wl_surface*, wl_array*) {}

    void PlatformWayland::keyboard_leave(void*, wl_keyboard*, u32, wl_surface*) {}

    void PlatformWayland::keyboard_key(void* data, wl_keyboard*, u32, u32, u32 key, u32 state) {
        PlatformWayland* platform = static_cast<PlatformWayland*>(data);
        const KeyCodeFlag keycode = translate_key(platform->m_xkb_keymap, platform->m_xkb_state, key);
        if (keycode != 0)
            InputSystem::process_key(keycode, state == WL_KEYBOARD_KEY_STATE_PRESSED);
    }

    void PlatformWayland::keyboard_modifiers(
        void* data,
        wl_keyboard*,
        u32,
        u32 mods_depressed,
        u32 mods_latched,
        u32 mods_locked,
        u32 group) {
        PlatformWayland* platform = static_cast<PlatformWayland*>(data);
        if (platform->m_xkb_state != nullptr) {
            xkb_state_update_mask(
                platform->m_xkb_state,
                mods_depressed,
                mods_latched,
                mods_locked,
                0,
                0,
                group);
        }
    }

    void PlatformWayland::keyboard_repeat_info(void*, wl_keyboard*, i32, i32) {}

    void PlatformWayland::pointer_enter(
        void*,
        wl_pointer*,
        u32,
        wl_surface*,
        wl_fixed_t surface_x,
        wl_fixed_t surface_y) {
        InputSystem::process_mouse_move(fixed_to_i16(surface_x), fixed_to_i16(surface_y));
    }

    void PlatformWayland::pointer_leave(void*, wl_pointer*, u32, wl_surface*) {}

    void PlatformWayland::pointer_motion(void*, wl_pointer*, u32, wl_fixed_t surface_x, wl_fixed_t surface_y) {
        InputSystem::process_mouse_move(fixed_to_i16(surface_x), fixed_to_i16(surface_y));
    }

    void PlatformWayland::pointer_button(void*, wl_pointer*, u32, u32, u32 button, u32 state) {
        MouseButton translated_button;
        switch (button) {
            case BTN_LEFT: translated_button = MouseButton::Left; break;
            case BTN_RIGHT: translated_button = MouseButton::Right; break;
            case BTN_MIDDLE: translated_button = MouseButton::Middle; break;
            default: return;
        }

        InputSystem::process_mouse_button(translated_button, state == WL_POINTER_BUTTON_STATE_PRESSED);
    }

    void PlatformWayland::pointer_axis(void*, wl_pointer*, u32, u32 axis, wl_fixed_t value) {
        if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL || value == 0)
            return;

        // Wayland's positive vertical direction is down; the engine uses positive for up.
        InputSystem::process_mouse_wheel(value < 0 ? 1 : -1);
    }

    void PlatformWayland::apply_pending_configure() {
        if (m_pending_width == m_width && m_pending_height == m_height)
            return;

        if (!m_ready_for_events) {
            m_width = m_pending_width;
            m_height = m_pending_height;
            return;
        }

        EventContext context{};
        context.data.u32[0] = m_pending_width;
        context.data.u32[1] = m_pending_height;
        EventSystem::fire_event(SystemEventCode::Resized, nullptr, context);
    }

    void PlatformWayland::release_keyboard() {
        if (m_keyboard != nullptr) {
            if (wl_keyboard_get_version(m_keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
                wl_keyboard_release(m_keyboard);
            else
                wl_keyboard_destroy(m_keyboard);
            m_keyboard = nullptr;
        }
        if (m_xkb_state != nullptr) {
            xkb_state_unref(m_xkb_state);
            m_xkb_state = nullptr;
        }
        if (m_xkb_keymap != nullptr) {
            xkb_keymap_unref(m_xkb_keymap);
            m_xkb_keymap = nullptr;
        }
    }

    void PlatformWayland::release_pointer() {
        if (m_pointer == nullptr)
            return;

        if (wl_pointer_get_version(m_pointer) >= WL_POINTER_RELEASE_SINCE_VERSION)
            wl_pointer_release(m_pointer);
        else
            wl_pointer_destroy(m_pointer);
        m_pointer = nullptr;
    }
}
