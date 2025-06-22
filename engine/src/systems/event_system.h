#pragma once

#include "collections/dyarr.h"

namespace nk {
    struct EventContext {
        union {
            i64 i64[2];
            u64 u64[2];
            f64 f64[2];

            i32 i32[4];
            u32 u32[4];
            f32 f32[4];

            i16 i16[8];
            u16 u16[8];

            i8 i8[16];
            u8 u8[16];

            char c[16];
        } data;
    };

    enum class SystemEventCode : u16 {
        ApplicationQuit = 0x01,
        KeyPressed = 0x02,
        KeyReleased = 0x03,
        ButtonPressed = 0x04,
        ButtonReleased = 0x05,
        MouseMoved = 0x06,
        MouseWheel = 0x07,
        Resized = 0x08,
        MaxEventCode = 0xFF
    };

    using PFN_OnEvent =
        bool (*)(SystemEventCode code,
                 void* sender,
                 void* listener,
                 EventContext context);

    struct RegisteredEvent {
        void* listener;
        PFN_OnEvent callback;
    };

    struct EventCodeEntry {
        cl::dyarr<RegisteredEvent> events;
    };

    class EventSystem {
    public:
        ~EventSystem() = default;

        static EventSystem& init();
        static void shutdown();

        static EventSystem& get() {
            static EventSystem instance;
            return instance;
        }

        static bool register_event(SystemEventCode code, void* listener, PFN_OnEvent callback);
        static bool unregister_event(SystemEventCode code, void* listener, PFN_OnEvent callback);
        static bool fire_event(SystemEventCode code, void* sender, const EventContext& ctx);

    private:
        EventSystem() = default;

        mem::Allocator* m_allocator;
        EventCodeEntry m_registered[static_cast<u16>(SystemEventCode::MaxEventCode)];
    };
}
