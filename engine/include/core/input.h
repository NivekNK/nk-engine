#pragma once

#include "core/input_codes.h"

namespace nk::Input {
    bool is_key_down(KeyCodeFlag keycode);

    bool is_key_up(KeyCodeFlag keycode);

    bool was_key_down(KeyCodeFlag keycode);

    bool was_key_up(KeyCodeFlag keycode);

    bool is_mouse_button_down(MouseButton button);

    bool is_mouse_button_up(MouseButton button);

    bool was_mouse_button_down(MouseButton button);

    bool was_mouse_button_up(MouseButton button);

    void get_mouse_position(i16& out_x, i16& out_y);

    void get_previous_mouse_position(i16& out_x, i16& out_y);
}
