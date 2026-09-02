#include <gtest/gtest.h>

#include "systems/memory_system.h"

TEST(MemorySystem, MemorySystemInit) {
#if NK_MEMORY_TRACKING_ENABLED
    auto& memory_system = NK_MEMORY_SYSTEM_INIT();
    EXPECT_EQ(memory_system.state(), nk::mem::MemorySystemState::Ready);

    nk::u32* data = native_allocate_lot(nk::u32, 5);
    ASSERT_NE(data, nullptr);
    native_free_lot(nk::u32, data, 5);

    NK_MEMORY_SYSTEM_SHUTDOWN();
    EXPECT_EQ(memory_system.state(), nk::mem::MemorySystemState::Stopped);
#endif
}
