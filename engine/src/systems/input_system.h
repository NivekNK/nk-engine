#pragma once

#include "core/input_codes.h"

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
            TraceLog("nk::InputSystem Initialized.");
            return get();
        }

        static void shutdown() {
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

        void process_key_impl(KeyCodeFlag keycode, bool pressed);
        void process_mouse_button_impl(MouseButton button, bool pressed);
        void process_mouse_move_impl(i16 x, i16 y);
        void process_mouse_wheel_impl(i8 z_delta);

        KeyboardState m_current_keyboard_state;
        KeyboardState m_previous_keyboard_state;

        MouseState m_current_mouse_state;
        MouseState m_previous_mouse_state;
    };
}
