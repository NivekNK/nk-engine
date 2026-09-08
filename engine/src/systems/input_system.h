#pragma once

#include "core/input.h"
#include "core/strbuf.h"

namespace nk {
    class InputSystem {
    public:
        struct KeyboardState {
            bool keys[256];
        };

        struct MouseState {
            i16 x;
            i16 y;
            bool buttons[static_cast<u8>(MouseButton::MaxButtons)];
        };

        ~InputSystem() = default;

        static InputSystem& init() {
            get().reset_impl();
            TraceLog("nk::InputSystem Initialized.");
            return get();
        }

        static void shutdown() {
            get().reset_impl();
            TraceLog("nk::InputSystem Shutdown.");
        }

        static InputSystem& get() {
            static InputSystem instance;
            return instance;
        }

        static void update(f64 delta_time) { get().update_impl(delta_time); }

        static void process_key(KeyCodeFlag keycode, bool pressed) { get().process_key_impl(keycode, pressed); }
        static void process_mouse_button(MouseButton button, bool pressed) { get().process_mouse_button_impl(button, pressed); }
        static void process_mouse_move(i16 x, i16 y) { get().process_mouse_move_impl(x, y); }
        static void process_mouse_wheel(i8 z_delta) { get().process_mouse_wheel_impl(z_delta); }
        static void process_key_repeat(KeyCodeFlag keycode) { get().process_key_repeat_impl(keycode); }
        [[nodiscard]] static result<void, text_input_error> process_text(strview text) {
            return get().process_text_impl(text);
        }
        [[nodiscard]] static result<void, text_input_error> process_preedit(
            strview text, u32 selection_begin, u32 selection_end) {
            return get().process_preedit_impl(text, selection_begin, selection_end);
        }
        static void process_text_delete(u32 before, u32 after) {
            get().process_text_delete_impl(before, after);
        }
        static void clear_keyboard() { get().clear_keyboard_impl(); }
        static void set_text_input_enabled(bool enabled) { get().set_text_input_enabled_impl(enabled); }
        static bool text_input_enabled() { return get().m_text_input_enabled; }
        static TextInputState text_input() { return get().text_input_impl(); }

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

        KeyboardState get_previous_keyboard_state() const { return m_previous_keyboard_state; }

    private:
        InputSystem() = default;

        void update_impl(f64 delta_time);
        void reset_impl();

        void process_key_impl(KeyCodeFlag keycode, bool pressed);
        void process_key_repeat_impl(KeyCodeFlag keycode);
        result<void, text_input_error> process_text_impl(strview text);
        result<void, text_input_error> process_preedit_impl(
            strview text, u32 selection_begin, u32 selection_end);
        void process_text_delete_impl(u32 before, u32 after);
        void clear_keyboard_impl();
        void set_text_input_enabled_impl(bool enabled);
        TextInputState text_input_impl() const;
        void process_mouse_button_impl(MouseButton button, bool pressed);
        void process_mouse_move_impl(i16 x, i16 y);
        void process_mouse_wheel_impl(i8 z_delta);

        KeyboardState m_current_keyboard_state;
        KeyboardState m_previous_keyboard_state;

        MouseState m_current_mouse_state;
        MouseState m_previous_mouse_state;
        strbuf<1024> m_committed_text;
        strbuf<255> m_preedit_text;
        u32 m_selection_begin = 0, m_selection_end = 0;
        u32 m_delete_before = 0, m_delete_after = 0;
        bool m_text_input_enabled = false;
        bool m_text_overflowed = false;
    };
}
