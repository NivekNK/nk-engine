#include "nkpch.h"

#include "systems/input_system.h"

#include "systems/event_system.h"

namespace nk {
    bool InputSystem::is_key_down(KeyCodeFlag keycode) {
        return m_current_keyboard_state.keys[keycode] == true;
    }

    bool InputSystem::is_key_up(KeyCodeFlag keycode) {
        return m_current_keyboard_state.keys[keycode] == false;
    }

    bool InputSystem::was_key_down(KeyCodeFlag keycode) {
        return m_previous_keyboard_state.keys[keycode] == true;
    }

    bool InputSystem::was_key_up(KeyCodeFlag keycode) {
        return m_previous_keyboard_state.keys[keycode] == false;
    }

    bool InputSystem::is_mouse_button_down(MouseButton button) {
        return m_current_mouse_state.buttons[static_cast<u8>(button)] == true;
    }

    bool InputSystem::is_mouse_button_up(MouseButton button) {
        return m_current_mouse_state.buttons[static_cast<u8>(button)] == false;
    }

    bool InputSystem::was_mouse_button_down(MouseButton button) {
        return m_previous_mouse_state.buttons[static_cast<u8>(button)] == true;
    }

    bool InputSystem::was_mouse_button_up(MouseButton button) {
        return m_previous_mouse_state.buttons[static_cast<u8>(button)] == false;
    }

    void InputSystem::get_mouse_position(i16& out_x, i16& out_y) {
        out_x = m_current_mouse_state.x;
        out_y = m_current_mouse_state.y;
    }

    void InputSystem::get_previous_mouse_position(i16& out_x, i16& out_y) {
        out_x = m_previous_mouse_state.x;
        out_y = m_previous_mouse_state.y;
    }

    void InputSystem::update(f64 delta_time) {
        memcpy(&m_previous_keyboard_state, &m_current_keyboard_state, sizeof(KeyboardState));
        memcpy(&m_previous_mouse_state, &m_current_mouse_state, sizeof(MouseState));
    }

    void InputSystem::process_key(KeyCodeFlag keycode, bool pressed) {
        if (m_current_keyboard_state.keys[keycode] == pressed)
            return;

        m_current_keyboard_state.keys[keycode] = pressed;

        const SystemEventCode code = pressed ? SystemEventCode::KeyPressed : SystemEventCode::KeyReleased;
        EventContext context;
        context.data.u16[0] = keycode;
        EventSystem::fire_event(code, nullptr, context);
    }

    void InputSystem::process_mouse_button(MouseButton button, bool pressed) {
        const u8 button_value = static_cast<u8>(button);

        if (m_current_mouse_state.buttons[button_value] == pressed)
            return;

        m_current_mouse_state.buttons[button_value] = pressed;

        const SystemEventCode code = pressed ? SystemEventCode::ButtonPressed : SystemEventCode::ButtonReleased;
        EventContext context;
        context.data.u8[0] = button_value;
        EventSystem::fire_event(code, nullptr, context);
    }

    void InputSystem::process_mouse_move(i16 x, i16 y) {
        if (m_current_mouse_state.x == x && m_current_mouse_state.y == y)
            return;

        m_current_mouse_state.x = x;
        m_current_mouse_state.y = y;

        EventContext context;
        context.data.i16[0] = x;
        context.data.i16[1] = y;
        EventSystem::fire_event(SystemEventCode::MouseMoved, nullptr, context);
    }

    void InputSystem::process_mouse_wheel(i8 z_delta) {
        EventContext context;
        context.data.i8[0] = z_delta;
        EventSystem::fire_event(SystemEventCode::MouseWheel, nullptr, context);
    }
}
