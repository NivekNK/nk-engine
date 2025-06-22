#include "nkpch.h"

#include "systems/event_system.h"
#include "memory/malloc_allocator.h"

namespace nk {
    EventSystem& EventSystem::init() {
        EventSystem& instance = get();

        instance.m_allocator = native_construct(mem::MallocAllocator);
        instance.m_allocator->allocator_init(mem::MallocAllocator, "EventSystem", MemoryType::Event);

        TraceLog("nk::EventSystem Initialized.");
        return instance;
    }

    void EventSystem::shutdown() {
        EventSystem& instance = get();

        const u16 max_event_codes = static_cast<u16>(SystemEventCode::MaxEventCode);
        for (u16 i = 0; i < max_event_codes; i++) {
            if (instance.m_registered[i].events.capacity() > 0) {
                instance.m_registered[i].events.dyarr_shutdown();
            }
        }

        native_deconstruct(mem::MallocAllocator, instance.m_allocator);
        TraceLog("nk::EventSystem Shutdown.");
    }

    bool EventSystem::register_event(SystemEventCode code, void* listener, PFN_OnEvent callback) {
        EventSystem& instance = get();

        const u16 code_value = static_cast<u16>(code);

        if (instance.m_registered[code_value].events.capacity() <= 0) {
            instance.m_registered[code_value].events.dyarr_init(instance.m_allocator, 4);
        }

        const u64 registered_count = instance.m_registered[code_value].events.length();
        for (u64 i = 0; i < registered_count; i++) {
            if (instance.m_registered[code_value].events[i].listener == listener) {
                return false;
            }
        }

        instance.m_registered[code_value].events.dyarr_push_copy(RegisteredEvent{
            .listener = listener,
            .callback = callback,
        });

        return true;
    }

    bool EventSystem::unregister_event(SystemEventCode code, void* listener, PFN_OnEvent callback) {
        EventSystem& instance = get();

        const u16 code_value = static_cast<u16>(code);

        if (instance.m_registered[code_value].events.empty()) {
            return false;
        }

        const u64 registered_count = instance.m_registered[code_value].events.length();
        for (u64 i = 0; i < registered_count; i++) {
            RegisteredEvent& event = instance.m_registered[code_value].events[i];
            if (event.listener == listener && event.callback == callback) {
                instance.m_registered[code_value].events.dyarr_remove(i);
                return true;
            }
        }

        return false;
    }

    bool EventSystem::fire_event(SystemEventCode code, void* sender, const EventContext& ctx) {
        EventSystem& instance = get();

        const u16 code_value = static_cast<u16>(code);

        if (instance.m_registered[code_value].events.empty()) {
            return false;
        }

        const u64 registered_count = instance.m_registered[code_value].events.length();
        for (u64 i = 0; i < registered_count; i++) {
            RegisteredEvent& event = instance.m_registered[code_value].events[i];
            if (event.callback(code, sender, event.listener, ctx)) {
                return true;
            }
        }

        return false;
    }
}
