#pragma once

#include "memory/allocator.h"
#include "nk/input_codes.h"
#include "event/action.h"

namespace nk {
    class EventSystem {
    public:
        ~EventSystem() = default;

        static EventSystem& init(Allocator* allocator) { 
            EventSystem& system = get();
            system.window_close.init(allocator);
            system.window_resize.init(allocator);
            system.window_focus.init(allocator);
            system.window_moved.init(allocator);
            system.key_state_changed.init(allocator);
            system.mouse_button_changed.init(allocator);
            system.mouse_position_changed.init(allocator);
            return system;
        }

        static void shutdown() {
            EventSystem& system = get();
            system.window_close.free();
            system.window_resize.free();
            system.window_focus.free();
            system.window_moved.free();
            system.key_state_changed.free();
            system.mouse_button_changed.free();
            system.mouse_position_changed.free();
        }

        static EventSystem& get() {
            static EventSystem instance;
            return instance;
        }

        Action<> window_close;
        Action<u16, u16> window_resize;
        Action<bool> window_focus;
        Action<i16, i16> window_moved;
        Action<KeyState> key_state_changed;
        Action<MouseButtonState> mouse_button_changed;
        Action<i16, i16> mouse_position_changed;

    private:
        EventSystem() = default;
    };
}