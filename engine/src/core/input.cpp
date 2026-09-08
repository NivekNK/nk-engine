#include "nkpch.h"

#include "core/input.h"

#include "systems/input_system.h"

namespace nk::Input {
    bool is_key_down(KeyCodeFlag keycode) {
        return InputSystem::get().is_key_down(keycode);
    }

    bool is_key_up(KeyCodeFlag keycode) {
        return InputSystem::get().is_key_up(keycode);
    }

    bool was_key_down(KeyCodeFlag keycode) {
        return InputSystem::get().was_key_down(keycode);
    }

    bool was_key_up(KeyCodeFlag keycode) {
        return InputSystem::get().was_key_up(keycode);
    }

    bool is_mouse_button_down(MouseButton button) {
        return InputSystem::get().is_mouse_button_down(button);
    }

    bool is_mouse_button_up(MouseButton button) {
        return InputSystem::get().is_mouse_button_up(button);
    }

    bool was_mouse_button_down(MouseButton button) {
        return InputSystem::get().was_mouse_button_down(button);
    }

    bool was_mouse_button_up(MouseButton button) {
        return InputSystem::get().was_mouse_button_up(button);
    }

    void get_mouse_position(i16& out_x, i16& out_y) {
        InputSystem::get().get_mouse_position(out_x, out_y);
    }

    void get_previous_mouse_position(i16& out_x, i16& out_y) {
        InputSystem::get().get_previous_mouse_position(out_x, out_y);
    }

    void set_text_input_enabled(const bool enabled) {
        InputSystem::set_text_input_enabled(enabled);
    }

    bool text_input_enabled() { return InputSystem::text_input_enabled(); }

    TextInputState text_input() { return InputSystem::text_input(); }
}
