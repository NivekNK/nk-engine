#include <gtest/gtest.h>

#include "systems/event_system.h"
#include "systems/input_system.h"
#include "systems/memory_system.h"

namespace {
    struct ListenerProbe {
        nk::u32 first_calls = 0;
        nk::u32 second_calls = 0;
        nk::i8 wheel_delta = 0;
    };

    bool first_callback(
        nk::SystemEventCode,
        void*,
        void* listener,
        nk::EventContext) {
        ++static_cast<ListenerProbe*>(listener)->first_calls;
        return false;
    }

    bool second_callback(
        nk::SystemEventCode,
        void*,
        void* listener,
        nk::EventContext context) {
        auto& probe = *static_cast<ListenerProbe*>(listener);
        ++probe.second_calls;
        probe.wheel_delta = context.data.i8[0];
        return true;
    }
}

TEST(EventSystem, DistinguishesCallbacksOwnedByTheSameListener) {
    nk::EventSystem::shutdown();
    NK_MEMORY_SYSTEM_INIT();
    nk::EventSystem::init();

    ListenerProbe listener{};
    EXPECT_TRUE(nk::EventSystem::register_event(
        nk::SystemEventCode::MouseWheel,
        &listener,
        first_callback));
    EXPECT_TRUE(nk::EventSystem::register_event(
        nk::SystemEventCode::MouseWheel,
        &listener,
        second_callback));
    EXPECT_FALSE(nk::EventSystem::register_event(
        nk::SystemEventCode::MouseWheel,
        &listener,
        first_callback));

    nk::InputSystem::process_mouse_wheel(-1);
    EXPECT_EQ(listener.first_calls, 1u);
    EXPECT_EQ(listener.second_calls, 1u);
    EXPECT_EQ(listener.wheel_delta, -1);

    EXPECT_TRUE(nk::EventSystem::unregister_event(
        nk::SystemEventCode::MouseWheel,
        &listener,
        first_callback));
    EXPECT_FALSE(nk::EventSystem::unregister_event(
        nk::SystemEventCode::MouseWheel,
        &listener,
        first_callback));
    EXPECT_TRUE(nk::EventSystem::unregister_event(
        nk::SystemEventCode::MouseWheel,
        &listener,
        second_callback));

    nk::EventSystem::shutdown();
    NK_MEMORY_SYSTEM_SHUTDOWN();
}
