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

        void update(f64 delta_time);

        void process_key(KeyCodeFlag keycode, bool pressed);
        void process_mouse_button(MouseButton button, bool pressed);
        void process_mouse_move(i16 x, i16 y);
        void process_mouse_wheel(i8 z_delta);

        KeyboardState get_previous_keyboard_state() const { return m_previous_keyboard_state; }

    private:
        InputSystem() = default;

        KeyboardState m_current_keyboard_state;
        KeyboardState m_previous_keyboard_state;

        MouseState m_current_mouse_state;
        MouseState m_previous_mouse_state;
    };
}
