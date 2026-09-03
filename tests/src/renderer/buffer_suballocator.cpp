#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"
#include "renderer/buffer_suballocator.h"

TEST(BufferSuballocator, ReportsLifecycleAndOperationErrors) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    nk::BufferSuballocator ranges;

    auto unavailable = ranges.reserve(16, 4);
    ASSERT_FALSE(unavailable);
    EXPECT_EQ(
        unavailable.error().code,
        nk::renderer_error_code::buffer_allocation_failed);
    EXPECT_EQ(
        unavailable.error().native_code,
        static_cast<nk::i32>(nk::mem::free_list_error::not_initialized));

    auto invalid = ranges.init(metadata, 0, 4);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(
        invalid.error().code,
        nk::renderer_error_code::buffer_allocation_failed);
    EXPECT_FALSE(ranges.initialized());

    ASSERT_TRUE(ranges.init(metadata, 256, 8));
    EXPECT_TRUE(ranges.initialized());
    EXPECT_EQ(ranges.size(), 256);
    EXPECT_EQ(ranges.free_space(), 256);
    EXPECT_EQ(ranges.occupied_space(), 0);

    auto duplicate = ranges.init(metadata, 256, 8);
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(
        duplicate.error().native_code,
        static_cast<nk::i32>(
            nk::mem::free_list_error::already_initialized));
}

TEST(BufferSuballocator, ReusesOffsetZeroAndPreservesAlignment) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    nk::BufferSuballocator ranges;
    ASSERT_TRUE(ranges.init(metadata, 512, 16));

    auto first = ranges.reserve(48, 16);
    auto second = ranges.reserve(32, 64);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(first->offset, 0);
    EXPECT_EQ(first->size, 48);
    EXPECT_EQ(second->offset % 64, 0);
    EXPECT_EQ(ranges.occupied_space(), 80);

    ASSERT_TRUE(ranges.release(*first));
    auto reused = ranges.reserve(48, 16);
    ASSERT_TRUE(reused);
    EXPECT_EQ(reused->offset, first->offset);
    EXPECT_EQ(reused->size, first->size);

    EXPECT_TRUE(ranges.release(*second));
    EXPECT_TRUE(ranges.release(*reused));
    EXPECT_EQ(ranges.free_space(), 512);
}

TEST(BufferSuballocator, KeepsIndependentBufferAddressSpaces) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    nk::BufferSuballocator vertices;
    nk::BufferSuballocator indices;
    ASSERT_TRUE(vertices.init(metadata, 1024, 16));
    ASSERT_TRUE(indices.init(metadata, 256, 16));

    auto vertex_range = vertices.reserve(96, 16);
    auto index_range = indices.reserve(24, alignof(nk::u32));
    ASSERT_TRUE(vertex_range);
    ASSERT_TRUE(index_range);
    EXPECT_EQ(vertex_range->offset, 0);
    EXPECT_EQ(index_range->offset, 0);
    EXPECT_EQ(vertices.free_space(), 928);
    EXPECT_EQ(indices.free_space(), 232);

    EXPECT_TRUE(indices.release(*index_range));
    EXPECT_TRUE(vertices.release(*vertex_range));
}

TEST(BufferSuballocator, ResizesWithoutMovingLiveRanges) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    nk::BufferSuballocator ranges;
    ASSERT_TRUE(ranges.init(metadata, 128, 8));

    auto live = ranges.reserve(64, 16);
    ASSERT_TRUE(live);
    const nk::mem::MemoryRange original = *live;

    ASSERT_TRUE(ranges.resize(256));
    EXPECT_EQ(live->offset, original.offset);
    EXPECT_EQ(live->size, original.size);
    EXPECT_EQ(ranges.size(), 256);
    EXPECT_EQ(ranges.occupied_space(), 64);

    auto blocked = ranges.resize(32);
    ASSERT_FALSE(blocked);
    EXPECT_EQ(blocked.error().code, nk::renderer_error_code::buffer_resize_failed);
    EXPECT_EQ(
        blocked.error().native_code,
        static_cast<nk::i32>(nk::mem::free_list_error::shrink_blocked));
    EXPECT_EQ(ranges.size(), 256);
    EXPECT_EQ(ranges.occupied_space(), 64);

    ASSERT_TRUE(ranges.release(original));
    ASSERT_TRUE(ranges.resize(32));
    EXPECT_EQ(ranges.size(), 32);
    EXPECT_EQ(ranges.free_space(), 32);
}

TEST(BufferSuballocator, RecyclesGeometryRangesAcrossManyLifetimes) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    nk::BufferSuballocator ranges;
    ASSERT_TRUE(ranges.init(metadata, 4096, 34));

    for (nk::u64 cycle = 0; cycle < 256; ++cycle) {
        nk::mem::MemoryRange allocations[16]{};
        for (nk::u64 index = 0; index < 16; ++index) {
            auto reserved = ranges.reserve(32 + index, 16);
            ASSERT_TRUE(reserved);
            allocations[index] = *reserved;
        }
        for (nk::u64 index = 0; index < 16; index += 2)
            ASSERT_TRUE(ranges.release(allocations[index]));
        for (nk::u64 index = 1; index < 16; index += 2)
            ASSERT_TRUE(ranges.release(allocations[index]));
        ASSERT_EQ(ranges.free_space(), 4096);
    }
}

TEST(BufferSuballocator, DoesNotAllocateMetadataAfterInitialization) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    nk::BufferSuballocator ranges;
    ASSERT_TRUE(ranges.init(metadata, 1024, 32));
    ASSERT_EQ(metadata.get_active_allocation_count(), 1);
    const nk::u64 metadata_peak = metadata.get_peak_used_bytes();

    for (nk::u64 index = 0; index < 100; ++index) {
        auto reserved = ranges.reserve(64, 64);
        ASSERT_TRUE(reserved);
        ASSERT_TRUE(ranges.release(*reserved));
    }

    EXPECT_EQ(metadata.get_active_allocation_count(), 1);
    EXPECT_EQ(metadata.get_peak_used_bytes(), metadata_peak);
    ranges.shutdown();
    EXPECT_EQ(metadata.get_active_allocation_count(), 0);
}
