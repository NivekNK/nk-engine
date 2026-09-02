#include "nkpch.h"

#include "platform/platform_linux.h"

#include "core/app.h"
#include "systems/event_system.h"
#include "systems/input_system.h"

#include <X11/keysym.h>

namespace {
    xcb_atom_t intern_atom(xcb_connection_t* connection, const char* name) {
        const auto cookie = xcb_intern_atom(connection, false, std::strlen(name), name);
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, nullptr);
        if (reply == nullptr)
            return XCB_ATOM_NONE;

        const xcb_atom_t atom = reply->atom;
        std::free(reply);
        return atom;
    }

    nk::KeyCodeFlag translate_key(xcb_key_symbols_t* symbols, xcb_keycode_t keycode) {
        const xcb_keysym_t keysym = xcb_key_symbols_get_keysym(symbols, keycode, 0);

        if (keysym >= XK_a && keysym <= XK_z)
            return nk::KeyCode::A + static_cast<nk::KeyCodeFlag>(keysym - XK_a);
        if (keysym >= XK_A && keysym <= XK_Z)
            return nk::KeyCode::A + static_cast<nk::KeyCodeFlag>(keysym - XK_A);
        if (keysym >= XK_0 && keysym <= XK_9)
            return nk::KeyCode::Num0 + static_cast<nk::KeyCodeFlag>(keysym - XK_0);
        if (keysym >= XK_F1 && keysym <= XK_F24)
            return nk::KeyCode::F1 + static_cast<nk::KeyCodeFlag>(keysym - XK_F1);
        if (keysym >= XK_KP_0 && keysym <= XK_KP_9)
            return nk::KeyCode::Numpad0 + static_cast<nk::KeyCodeFlag>(keysym - XK_KP_0);

        switch (keysym) {
            case XK_BackSpace: return nk::KeyCode::Backspace;
            case XK_Return:
            case XK_KP_Enter: return nk::KeyCode::Enter;
            case XK_Tab:
            case XK_ISO_Left_Tab: return nk::KeyCode::Tab;
            case XK_Shift_L: return nk::KeyCode::LShift;
            case XK_Shift_R: return nk::KeyCode::RShift;
            case XK_Control_L: return nk::KeyCode::LCtrl;
            case XK_Control_R: return nk::KeyCode::RCtrl;
            case XK_Alt_L:
            case XK_Meta_L: return nk::KeyCode::LAlt;
            case XK_Alt_R:
            case XK_Meta_R:
            case XK_ISO_Level3_Shift: return nk::KeyCode::RAlt;
            case XK_Pause: return nk::KeyCode::Pause;
            case XK_Caps_Lock: return nk::KeyCode::Capital;
            case XK_Escape: return nk::KeyCode::Escape;
            case XK_space: return nk::KeyCode::Space;
            case XK_Page_Up: return nk::KeyCode::PageUp;
            case XK_Page_Down: return nk::KeyCode::PageDown;
            case XK_End: return nk::KeyCode::End;
            case XK_Home: return nk::KeyCode::Home;
            case XK_Left: return nk::KeyCode::Left;
            case XK_Up: return nk::KeyCode::Up;
            case XK_Right: return nk::KeyCode::Right;
            case XK_Down: return nk::KeyCode::Down;
            case XK_Print: return nk::KeyCode::PrintScreen;
            case XK_Insert: return nk::KeyCode::Insert;
            case XK_Delete: return nk::KeyCode::Delete;
            case XK_Super_L: return nk::KeyCode::LSuper;
            case XK_Super_R: return nk::KeyCode::RSuper;
            case XK_Menu: return nk::KeyCode::Apps;
            case XK_Num_Lock: return nk::KeyCode::NumLock;
            case XK_Scroll_Lock: return nk::KeyCode::Scroll;
            case XK_KP_Multiply: return nk::KeyCode::Multiply;
            case XK_KP_Add: return nk::KeyCode::Add;
            case XK_KP_Separator: return nk::KeyCode::Separator;
            case XK_KP_Subtract: return nk::KeyCode::Subtract;
            case XK_KP_Decimal: return nk::KeyCode::Secimal;
            case XK_KP_Divide: return nk::KeyCode::Divide;
            case XK_KP_Equal: return nk::KeyCode::NumpadEqual;
            case XK_semicolon: return nk::KeyCode::Semicolon;
            case XK_apostrophe: return nk::KeyCode::Apostrophe;
            case XK_equal: return nk::KeyCode::Equal;
            case XK_comma: return nk::KeyCode::Comma;
            case XK_minus: return nk::KeyCode::Minus;
            case XK_period: return nk::KeyCode::Period;
            case XK_slash: return nk::KeyCode::Slash;
            case XK_grave: return nk::KeyCode::Grave;
            case XK_bracketleft: return nk::KeyCode::LBracket;
            case XK_backslash: return nk::KeyCode::Backslash;
            case XK_bracketright: return nk::KeyCode::RBracket;
            default: return 0;
        }
    }

    bool translate_mouse_button(uint8_t detail, nk::MouseButton* out_button) {
        switch (detail) {
            case XCB_BUTTON_INDEX_1:
                *out_button = nk::MouseButton::Left;
                return true;
            case XCB_BUTTON_INDEX_2:
                *out_button = nk::MouseButton::Middle;
                return true;
            case XCB_BUTTON_INDEX_3:
                *out_button = nk::MouseButton::Right;
                return true;
            default:
                return false;
        }
    }
}

namespace nk {
    PlatformLinux::PlatformLinux(const ApplicationConfig& config)
        : Platform(config),
          m_connection{nullptr},
          m_screen{nullptr},
          m_key_symbols{nullptr},
          m_window{XCB_WINDOW_NONE},
          m_wm_protocols{XCB_ATOM_NONE},
          m_wm_delete_window{XCB_ATOM_NONE},
          m_start_time{std::chrono::steady_clock::now()} {
        int screen_number = 0;
        m_connection = xcb_connect(nullptr, &screen_number);
        if (m_connection == nullptr || xcb_connection_has_error(m_connection) != 0) {
            FatalLog("Unable to connect to the X server. Check the DISPLAY environment variable.");
            m_running = false;
            return;
        }

        const xcb_setup_t* setup = xcb_get_setup(m_connection);
        xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(setup);
        for (int i = 0; i < screen_number; ++i)
            xcb_screen_next(&screen_iterator);
        m_screen = screen_iterator.data;
        if (m_screen == nullptr) {
            FatalLog("Unable to obtain the XCB screen.");
            m_running = false;
            return;
        }

        m_window = xcb_generate_id(m_connection);
        const uint32_t event_mask =
            XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_KEY_PRESS |
            XCB_EVENT_MASK_KEY_RELEASE |
            XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION |
            XCB_EVENT_MASK_FOCUS_CHANGE;
        const uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        const uint32_t value_list[] = {m_screen->black_pixel, event_mask};

        const auto create_cookie = xcb_create_window_checked(
            m_connection,
            XCB_COPY_FROM_PARENT,
            m_window,
            m_screen->root,
            config.start_pos_x,
            config.start_pos_y,
            config.start_width,
            config.start_height,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            m_screen->root_visual,
            value_mask,
            value_list);
        if (xcb_generic_error_t* error = xcb_request_check(m_connection, create_cookie)) {
            ErrorLog("XCB window creation failed with error code {}.", error->error_code);
            std::free(error);
            m_window = XCB_WINDOW_NONE;
            m_running = false;
            return;
        }

        xcb_change_property(
            m_connection,
            XCB_PROP_MODE_REPLACE,
            m_window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            config.name.size(),
            config.name.data());

        m_wm_protocols = intern_atom(m_connection, "WM_PROTOCOLS");
        m_wm_delete_window = intern_atom(m_connection, "WM_DELETE_WINDOW");
        if (m_wm_protocols != XCB_ATOM_NONE && m_wm_delete_window != XCB_ATOM_NONE) {
            xcb_change_property(
                m_connection,
                XCB_PROP_MODE_REPLACE,
                m_window,
                m_wm_protocols,
                XCB_ATOM_ATOM,
                32,
                1,
                &m_wm_delete_window);
        }

        m_key_symbols = xcb_key_symbols_alloc(m_connection);
        const auto map_cookie = xcb_map_window_checked(m_connection, m_window);
        if (xcb_generic_error_t* error = xcb_request_check(m_connection, map_cookie)) {
            ErrorLog("XCB window mapping failed with error code {}.", error->error_code);
            std::free(error);
            m_running = false;
            return;
        }

        xcb_flush(m_connection);
        InfoLog("PlatformLinux created using XCB.");
    }

    PlatformLinux::~PlatformLinux() {
        if (m_key_symbols != nullptr)
            xcb_key_symbols_free(m_key_symbols);

        if (m_connection != nullptr) {
            if (m_window != XCB_WINDOW_NONE)
                xcb_destroy_window(m_connection, m_window);
            xcb_flush(m_connection);
            xcb_disconnect(m_connection);
        }

        InfoLog("PlatformLinux destroyed.");
    }

    bool PlatformLinux::pump_messages() {
        if (m_connection == nullptr || xcb_connection_has_error(m_connection) != 0)
            return false;

        xcb_generic_event_t* event = nullptr;
        while ((event = xcb_poll_for_event(m_connection)) != nullptr) {
            const uint8_t event_type = event->response_type & ~0x80;
            switch (event_type) {
                case XCB_CLIENT_MESSAGE: {
                    const auto* client_message = reinterpret_cast<xcb_client_message_event_t*>(event);
                    if (client_message->type == m_wm_protocols &&
                        client_message->data.data32[0] == m_wm_delete_window) {
                        EventSystem::fire_event(SystemEventCode::ApplicationQuit, nullptr, EventContext{});
                    }
                } break;
                case XCB_DESTROY_NOTIFY:
                    EventSystem::fire_event(SystemEventCode::ApplicationQuit, nullptr, EventContext{});
                    break;
                case XCB_CONFIGURE_NOTIFY: {
                    const auto* configure = reinterpret_cast<xcb_configure_notify_event_t*>(event);
                    EventContext context{};
                    context.data.u32[0] = configure->width;
                    context.data.u32[1] = configure->height;
                    EventSystem::fire_event(SystemEventCode::Resized, nullptr, context);
                } break;
                case XCB_MAP_NOTIFY:
                    set_suspended(false);
                    break;
                case XCB_UNMAP_NOTIFY:
                    set_suspended(true);
                    break;
                case XCB_KEY_PRESS:
                case XCB_KEY_RELEASE: {
                    const auto* key = reinterpret_cast<xcb_key_press_event_t*>(event);
                    const KeyCodeFlag keycode = translate_key(m_key_symbols, key->detail);
                    if (keycode != 0)
                        InputSystem::process_key(keycode, event_type == XCB_KEY_PRESS);
                } break;
                case XCB_MOTION_NOTIFY: {
                    const auto* motion = reinterpret_cast<xcb_motion_notify_event_t*>(event);
                    InputSystem::process_mouse_move(motion->event_x, motion->event_y);
                } break;
                case XCB_BUTTON_PRESS: {
                    const auto* button = reinterpret_cast<xcb_button_press_event_t*>(event);
                    if (button->detail == 4 || button->detail == 5) {
                        InputSystem::process_mouse_wheel(button->detail == 4 ? 1 : -1);
                        break;
                    }

                    MouseButton mouse_button;
                    if (translate_mouse_button(button->detail, &mouse_button))
                        InputSystem::process_mouse_button(mouse_button, true);
                } break;
                case XCB_BUTTON_RELEASE: {
                    const auto* button = reinterpret_cast<xcb_button_release_event_t*>(event);
                    MouseButton mouse_button;
                    if (translate_mouse_button(button->detail, &mouse_button))
                        InputSystem::process_mouse_button(mouse_button, false);
                } break;
            }

            std::free(event);
        }

        return xcb_connection_has_error(m_connection) == 0;
    }

    f64 PlatformLinux::get_absolute_time() {
        return std::chrono::duration<f64>(std::chrono::steady_clock::now() - m_start_time).count();
    }

    void PlatformLinux::sleep(u64 ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}
