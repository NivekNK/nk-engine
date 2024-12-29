#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"

TEST(MallocAllocator, MallocAllocatorInit) {
    nk::MallocAllocator allocator;
    allocator.allocator_init(nk::MallocAllocator);

    EXPECT_EQ(allocator.size_bytes(), 0);
    EXPECT_EQ(allocator.used_bytes(), 0);
    EXPECT_EQ(allocator.allocation_count(), 0);
    EXPECT_EQ(allocator.data(), nullptr);
}