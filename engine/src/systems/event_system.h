#pragma once

#include "collections/dyarr.h"

#include <cstdint>

namespace nk {
    struct EventContext {
        union {
            std::int64_t i64[2];
            std::uint64_t u64[2];
            double f64[2];

            std::int32_t i32[4];
            std::uint32_t u32[4];
            float f32[4];

            std::int16_t i16[8];
            std::uint16_t u16[8];

            std::int8_t i8[16];
            std::uint8_t u8[16];

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
        SetRenderViewMode = 0x09,
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

        mem::Allocator* m_allocator = nullptr;
        EventCodeEntry m_registered[static_cast<u16>(SystemEventCode::MaxEventCode)];
    };
}
