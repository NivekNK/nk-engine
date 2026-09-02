#include <gtest/gtest.h>

#include "systems/memory_system.h"

TEST(MemorySystem, MemorySystemInit) {
#if NK_MEMORY_TRACKING_ENABLED
    auto& memory_system = NK_MEMORY_SYSTEM_INIT();
    EXPECT_EQ(memory_system.state(), nk::mem::MemorySystemState::Ready);
    EXPECT_EQ(memory_system.reentrant_event_count(), 0u);
    EXPECT_EQ(memory_system.metadata_failure_count(), 0u);
    const nk::u64 allocation_events = memory_system.allocation_event_count();

    nk::u32* data = native_allocate_lot(nk::u32, 5);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(memory_system.allocation_event_count(), allocation_events + 1);
    native_free_lot(nk::u32, data, 5);
    EXPECT_EQ(memory_system.allocation_event_count(), allocation_events + 1);

    NK_MEMORY_SYSTEM_SHUTDOWN();
    EXPECT_EQ(memory_system.state(), nk::mem::MemorySystemState::Stopped);
#endif
}
