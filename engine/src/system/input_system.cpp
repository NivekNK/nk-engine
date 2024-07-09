#include "nkpch.h"

#include "system/input_system.h"

#include "system/event_system.h"

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

        EventSystem::get().key_state_changed.invoke({
            .keycode = keycode,
            .pressed = pressed,
        });
    }

    void InputSystem::process_mouse_button(MouseButton button, bool pressed) {
        const u8 index = static_cast<u8>(button);
        if (m_current_mouse_state.buttons[index] == pressed)
            return;

        m_current_mouse_state.buttons[index] = pressed;

        EventSystem::get().mouse_button_changed.invoke({
            .button = button,
            .pressed = pressed,
        });
    }

    void InputSystem::process_mouse_move(i16 x, i16 y) {
        if (m_current_mouse_state.x == x && m_current_mouse_state.y == y)
            return;

        m_current_mouse_state.x = x;
        m_current_mouse_state.y = y;

        EventSystem::get().mouse_position_changed.invoke(x, y);
    }
}