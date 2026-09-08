#pragma once

#include "core/input_codes.h"
#include "core/result.h"
#include "core/strview.h"

namespace nk {
    enum class text_input_error : u8 {
        disabled,
        invalid_utf8,
        capacity_exceeded,
        invalid_selection,
    };

    // Views remain valid until the next input update or native text event.
    // Offsets and deletion lengths are UTF-8 byte counts, matching Clay and
    // the engine string types instead of platform UTF-16/codepoint indices.
    struct TextInputState {
        strview committed;
        strview preedit;
        u32 selection_begin = 0;
        u32 selection_end = 0;
        u32 delete_before = 0;
        u32 delete_after = 0;
        bool overflowed = false;
    };
}

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

    void set_text_input_enabled(bool enabled);
    bool text_input_enabled();
    TextInputState text_input();
}
