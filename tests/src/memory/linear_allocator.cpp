#include <gtest/gtest.h>

#include "systems/memory_system.h"
#include "memory/linear_allocator.h"
#include "systems/logging_system.h"

TEST(LinearAllocator, LinearAllocatorInit) {
    NK_MEMORY_SYSTEM_INIT();

    {
        DebugLog("Initializing Linear Allocator Test");
        nk::mem::Allocator* testing;
        nk::mem::LinearAllocator allocator;
        allocator.allocator_init(nk::mem::LinearAllocator, "TestLinearAllocator", nk::MemoryType::Test, KiB(1), nullptr);

        testing = &allocator;

        NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();

        auto _ = testing->allocate_raw(500.0f, 0.0f);

        EXPECT_FLOAT_EQ(allocator.get_used_bytes(), 500);
        NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();

        // Linear allocator frees all memory at once, individual allocations are not tracked as freed
        // This is expected behavior for linear allocators
        allocator._free_linear_allocator(__FILE__, __LINE__);
        DebugLog("Finishing Linear Allocator Test");
    }

    // After the allocator goes out of scope, the destructor will free the underlying memory
    // The individual 500-byte allocation will still show as leaked in the memory system
    // because linear allocators don't track individual deallocations
    NK_MEMORY_SYSTEM_DETAILED_LOG_REPORT();

    NK_MEMORY_SYSTEM_SHUTDOWN();
}
