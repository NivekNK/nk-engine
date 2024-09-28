#include <gtest/gtest.h>

#include "memory/linear_allocator.h"

TEST(LinearAllocator, LinearAllocatorShouldCreateAndFree) {
    nk::LinearAllocator allocator;
    allocator.allocator_init("Test", nk::MemoryType::None, sizeof(nk::u64) * 8, nullptr);

    EXPECT_NE(nullptr, allocator.start());
    EXPECT_EQ(sizeof(nk::u64) * 8, allocator.size());
    EXPECT_EQ(0, allocator.used());

    allocator.free_all();

    EXPECT_EQ(nullptr, allocator.start());
    EXPECT_NE(sizeof(nk::u64) * 8, allocator.size());
    EXPECT_EQ(0, allocator.used());
}

TEST(LinearAllocator, LinearAllocatorMultiAllocationAllSpace) {
    constexpr nk::u64 max_allocations = 1024;
    nk::LinearAllocator allocator;
    allocator.allocator_init("Test", nk::MemoryType::None, sizeof(nk::u64) * max_allocations);

    void* block;
    for (nk::u64 i = 0; i < max_allocations; i++) {
        block = allocator.allocate_raw(sizeof(nk::u64), alignof(nk::u64));
        EXPECT_NE(nullptr, block);
        EXPECT_EQ(sizeof(nk::u64) * (i + 1), allocator.used());
    }

    allocator.free_all();

    EXPECT_EQ(nullptr, allocator.start());
    EXPECT_NE(sizeof(nk::u64) * max_allocations, allocator.size());
    EXPECT_EQ(0, allocator.used());
}

TEST(LinearAllocator, LinearAllocatorMultiAllocationOverAllocate) {
    constexpr nk::u64 max_allocations = 3;
    nk::LinearAllocator allocator;
    allocator.allocator_init("Test", nk::MemoryType::None, sizeof(nk::u64) * max_allocations);

    void* block;
    for (nk::u64 i = 0; i < max_allocations; i++) {
        block = allocator.allocate_raw(sizeof(nk::u64), alignof(nk::u64));
        EXPECT_NE(nullptr, block);
        EXPECT_EQ(sizeof(nk::u64) * (i + 1), allocator.used());
    }

    DebugLog("NOTE: Following error is expected.");
    block = allocator.allocate_raw(sizeof(nk::u64), alignof(nk::u64));
    EXPECT_EQ(nullptr, block);
    EXPECT_EQ(sizeof(nk::u64) * max_allocations, allocator.used());

    allocator.free_all();

    EXPECT_EQ(nullptr, allocator.start());
    EXPECT_NE(sizeof(nk::u64) * max_allocations, allocator.size());
    EXPECT_EQ(0, allocator.used());
}

