#include "nkpch.h"

#include "systems/input_system.h"

#include "systems/event_system.h"
#include "core/utf8.h"

namespace nk {
    bool InputSystem::is_key_down(KeyCodeFlag keycode) {
        if (keycode >= 256) return false;
        return m_current_keyboard_state.keys[keycode] == true;
    }

    bool InputSystem::is_key_up(KeyCodeFlag keycode) {
        if (keycode >= 256) return true;
        return m_current_keyboard_state.keys[keycode] == false;
    }

    bool InputSystem::was_key_down(KeyCodeFlag keycode) {
        if (keycode >= 256) return false;
        return m_previous_keyboard_state.keys[keycode] == true;
    }

    bool InputSystem::was_key_up(KeyCodeFlag keycode) {
        if (keycode >= 256) return true;
        return m_previous_keyboard_state.keys[keycode] == false;
    }

    bool InputSystem::is_mouse_button_down(MouseButton button) {
        if (static_cast<u8>(button) >= static_cast<u8>(MouseButton::MaxButtons))
            return false;
        return m_current_mouse_state.buttons[static_cast<u8>(button)] == true;
    }

    bool InputSystem::is_mouse_button_up(MouseButton button) {
        if (static_cast<u8>(button) >= static_cast<u8>(MouseButton::MaxButtons))
            return false;
        return m_current_mouse_state.buttons[static_cast<u8>(button)] == false;
    }

    bool InputSystem::was_mouse_button_down(MouseButton button) {
        if (static_cast<u8>(button) >= static_cast<u8>(MouseButton::MaxButtons))
            return false;
        return m_previous_mouse_state.buttons[static_cast<u8>(button)] == true;
    }

    bool InputSystem::was_mouse_button_up(MouseButton button) {
        if (static_cast<u8>(button) >= static_cast<u8>(MouseButton::MaxButtons))
            return false;
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

    void InputSystem::update_impl(f64 delta_time) {
        (void)delta_time;
        memcpy(&m_previous_keyboard_state, &m_current_keyboard_state, sizeof(KeyboardState));
        memcpy(&m_previous_mouse_state, &m_current_mouse_state, sizeof(MouseState));
        m_committed_text.clear();
        m_delete_before = m_delete_after = 0;
        m_text_overflowed = false;
    }

    void InputSystem::reset_impl() {
        std::memset(&m_current_keyboard_state, 0, sizeof(m_current_keyboard_state));
        std::memset(&m_previous_keyboard_state, 0, sizeof(m_previous_keyboard_state));
        std::memset(&m_current_mouse_state, 0, sizeof(m_current_mouse_state));
        std::memset(&m_previous_mouse_state, 0, sizeof(m_previous_mouse_state));
        m_committed_text.clear(); m_preedit_text.clear();
        m_selection_begin = m_selection_end = 0;
        m_delete_before = m_delete_after = 0;
        m_text_input_enabled = m_text_overflowed = false;
    }

    void InputSystem::process_key_impl(KeyCodeFlag keycode, bool pressed) {
        if (keycode >= 256) return;
        if (m_current_keyboard_state.keys[keycode] == pressed)
            return;

        m_current_keyboard_state.keys[keycode] = pressed;

        const SystemEventCode code = pressed ? SystemEventCode::KeyPressed : SystemEventCode::KeyReleased;
        EventContext context{};
        context.data.u16[0] = keycode;
        EventSystem::fire_event(code, nullptr, context);
    }

    void InputSystem::process_key_repeat_impl(const KeyCodeFlag keycode) {
        if (keycode >= 256 || !m_current_keyboard_state.keys[keycode]) return;
        EventContext context{};
        context.data.u16[0] = keycode;
        context.data.u8[2] = 1;
        EventSystem::fire_event(SystemEventCode::KeyPressed, nullptr, context);
    }

    result<void, text_input_error> InputSystem::process_text_impl(const strview text) {
        if (!m_text_input_enabled) return err(text_input_error::disabled);
        if (!utf8::valid(text)) return err(text_input_error::invalid_utf8);
        u64 offset = 0;
        while (offset < text.length()) {
            const auto codepoint = utf8::decode(text, offset);
            if (!codepoint) return err(text_input_error::invalid_utf8);
            if (codepoint->bytes > m_committed_text.capacity() - m_committed_text.length()) {
                m_text_overflowed = true;
                return err(text_input_error::capacity_exceeded);
            }
            (void)m_committed_text.append(text.substr(offset, codepoint->bytes));
            offset += codepoint->bytes;
        }
        return ok();
    }

    result<void, text_input_error> InputSystem::process_preedit_impl(
        const strview text, const u32 selection_begin, const u32 selection_end) {
        if (!m_text_input_enabled) return err(text_input_error::disabled);
        if (!utf8::valid(text)) return err(text_input_error::invalid_utf8);
        if (text.length() > m_preedit_text.capacity()) {
            m_text_overflowed = true;
            return err(text_input_error::capacity_exceeded);
        }
        auto boundary = [&](const u32 offset) {
            return offset <= text.length() &&
                (offset == 0 || offset == text.length() ||
                 (static_cast<u8>(text[offset]) & 0xc0) != 0x80);
        };
        if (!boundary(selection_begin) || !boundary(selection_end))
            return err(text_input_error::invalid_selection);
        (void)m_preedit_text.assign(text);
        m_selection_begin = selection_begin;
        m_selection_end = selection_end;
        return ok();
    }

    void InputSystem::process_text_delete_impl(const u32 before, const u32 after) {
        if (!m_text_input_enabled) return;
        m_delete_before = before;
        m_delete_after = after;
    }

    void InputSystem::clear_keyboard_impl() {
        for (u32 key = 0; key < 256; ++key)
            if (m_current_keyboard_state.keys[key]) process_key_impl(key, false);
    }

    void InputSystem::set_text_input_enabled_impl(const bool enabled) {
        if (m_text_input_enabled == enabled) return;
        m_text_input_enabled = enabled;
        m_committed_text.clear(); m_preedit_text.clear();
        m_selection_begin = m_selection_end = 0;
        m_delete_before = m_delete_after = 0;
        m_text_overflowed = false;
    }

    TextInputState InputSystem::text_input_impl() const {
        return {m_committed_text.view(), m_preedit_text.view(),
            m_selection_begin, m_selection_end, m_delete_before,
            m_delete_after, m_text_overflowed};
    }

    void InputSystem::process_mouse_button_impl(MouseButton button, bool pressed) {
        const u8 button_value = static_cast<u8>(button);
        if (button_value >= static_cast<u8>(MouseButton::MaxButtons))
            return;

        if (m_current_mouse_state.buttons[button_value] == pressed)
            return;

        m_current_mouse_state.buttons[button_value] = pressed;

        const SystemEventCode code = pressed ? SystemEventCode::ButtonPressed : SystemEventCode::ButtonReleased;
        EventContext context{};
        context.data.u8[0] = button_value;
        EventSystem::fire_event(code, nullptr, context);
    }

    void InputSystem::process_mouse_move_impl(i16 x, i16 y) {
        if (m_current_mouse_state.x == x && m_current_mouse_state.y == y)
            return;

        m_current_mouse_state.x = x;
        m_current_mouse_state.y = y;

        EventContext context{};
        context.data.i16[0] = x;
        context.data.i16[1] = y;
        EventSystem::fire_event(SystemEventCode::MouseMoved, nullptr, context);
    }

    void InputSystem::process_mouse_wheel_impl(i8 z_delta) {
        EventContext context{};
        context.data.i8[0] = z_delta;
        EventSystem::fire_event(SystemEventCode::MouseWheel, nullptr, context);
    }
}
