#include <gtest/gtest.h>

#include "systems/memory_system.h"
#include "memory/linear_allocator.h"

TEST(LinearAllocator, LinearAllocatorInit) {
    NK_MEMORY_SYSTEM_INIT();

    {
        nk::mem::Allocator* testing;
        nk::mem::LinearAllocator allocator;
        allocator.allocator_init(nk::mem::LinearAllocator, "TestLinearAllocator", nk::MemoryType::Test, KiB(1), nullptr);

        testing = &allocator;

        NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();

        auto _ = testing->allocate_raw(500.0f, 0.0f);

        EXPECT_FLOAT_EQ(allocator.get_used_bytes(), 500);

        allocator._free_linear_allocator(__FILE__, __LINE__);
    }

    NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();

    NK_MEMORY_SYSTEM_SHUTDOWN();
}
