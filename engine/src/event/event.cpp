#include "nkpch.h"

#include "event/event.h"

#include "system/event_system.h"

namespace nk::Event {
    namespace WindowClose {
        u32 add_listener(const FuncHandler<>::Func& func) {
            return EventSystem::get().window_close.add_listener(func);
        }

        bool remove_listener(const u32 id) {
            return EventSystem::get().window_close.remove_listener(id);
        }
    }

    namespace WindowResize {
        u32 add_listener(const FuncHandler<u16, u16>::Func& func) {
            return EventSystem::get().window_resize.add_listener(func);
        }

        bool remove_listener(const u32 id) {
            return EventSystem::get().window_resize.remove_listener(id);
        }
    }

    namespace WindowFocus {
        u32 add_listener(const FuncHandler<bool>::Func& func) {
            return EventSystem::get().window_focus.add_listener(func);
        }

        bool remove_listener(const u32 id) {
            return EventSystem::get().window_focus.remove_listener(id);
        }
    }

    namespace WindowMoved {
        u32 add_listener(const FuncHandler<i16, i16>::Func& func) {
            return EventSystem::get().window_moved.add_listener(func);
        }

        bool remove_listener(const u32 id) {
            return EventSystem::get().window_moved.remove_listener(id);
        }
    }

    namespace KeyStateChanged {
        u32 add_listener(const FuncHandler<KeyState>::Func& func) {
            return EventSystem::get().key_state_changed.add_listener(func);
        }

        bool remove_listener(const u32 id) {
            return EventSystem::get().key_state_changed.remove_listener(id);
        }
    }

    namespace MouseButtonChanged {
        u32 add_listener(const FuncHandler<MouseButtonState>::Func& func) {
            return EventSystem::get().mouse_button_changed.add_listener(func);
        }

        bool remove_listener(const u32 id) {
            return EventSystem::get().mouse_button_changed.remove_listener(id);
        }
    }

    namespace MousePositionChanged {
        u32 add_listener(const FuncHandler<i16, i16>::Func& func) {
            return EventSystem::get().mouse_position_changed.add_listener(func);
        }

        bool remove_listener(const u32 id) {
            return EventSystem::get().mouse_position_changed.remove_listener(id);
        }
    }
}