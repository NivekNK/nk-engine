#pragma once

#include "nk/input_codes.h"
#include "event/action.h"

namespace nk::Event {
    namespace WindowClose {
        u32 add_listener(const FuncHandler<>::Func& func);
        bool remove_listener(const u32 id);
    }

    namespace WindowResize {
        u32 add_listener(const FuncHandler<u16, u16>::Func& func);
        bool remove_listener(const u32 id);
    }

    namespace WindowFocus {
        u32 add_listener(const FuncHandler<bool>::Func& func);
        bool remove_listener(const u32 id);
    }

    namespace WindowMoved {
        u32 add_listener(const FuncHandler<i16, i16>::Func& func);
        bool remove_listener(const u32 id);
    }

    namespace KeyStateChanged {
        u32 add_listener(const FuncHandler<KeyState>::Func& func);
        bool remove_listener(const u32 id);
    }

    namespace MouseButtonChanged {
        u32 add_listener(const FuncHandler<MouseButtonState>::Func& func);
        bool remove_listener(const u32 id);
    }

    namespace MousePositionChanged {
        u32 add_listener(const FuncHandler<i16, i16>::Func& func);
        bool remove_listener(const u32 id);
    }
}