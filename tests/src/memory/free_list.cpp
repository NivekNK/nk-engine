#include <gtest/gtest.h>

#include "memory/free_list.h"
#include "memory/linear_allocator.h"
#include "memory/malloc_allocator.h"

namespace {
    using nk::mem::free_list_error;
    using nk::mem::FreeList;
    using nk::mem::MemoryRange;

    void expect_single_free_range(
        const FreeList& list,
        const nk::u64 offset,
        const nk::u64 size) {
        ASSERT_EQ(list.range_count(), 1);
        const MemoryRange* range = list.free_range(0);
        ASSERT_NE(range, nullptr);
        EXPECT_EQ(range->offset, offset);
        EXPECT_EQ(range->size, size);
    }
}

TEST(FreeList, RejectsOperationsBeforeInitialization) {
    FreeList list;

    auto reserved = list.reserve(1);
    ASSERT_FALSE(reserved);
    EXPECT_EQ(reserved.error(), free_list_error::not_initialized);

    auto released = list.release({0, 1});
    ASSERT_FALSE(released);
    EXPECT_EQ(released.error(), free_list_error::not_initialized);

    auto resized = list.resize(2);
    ASSERT_FALSE(resized);
    EXPECT_EQ(resized.error(), free_list_error::not_initialized);

    auto cleared = list.clear();
    ASSERT_FALSE(cleared);
    EXPECT_EQ(cleared.error(), free_list_error::not_initialized);
    EXPECT_EQ(list.free_range(0), nullptr);
}

TEST(FreeList, ValidatesInitializationAndSmallestRegion) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;

    auto zero_size = list.init(allocator, 0, 1);
    ASSERT_FALSE(zero_size);
    EXPECT_EQ(zero_size.error(), free_list_error::invalid_size);

    auto zero_metadata = list.init(allocator, 1, 0);
    ASSERT_FALSE(zero_metadata);
    EXPECT_EQ(zero_metadata.error(), free_list_error::metadata_exhausted);

    auto overflow = list.init(allocator, 1, nk::numeric::u64_max);
    ASSERT_FALSE(overflow);
    EXPECT_EQ(overflow.error(), free_list_error::capacity_overflow);

    ASSERT_TRUE(list.init(allocator, 1, 1));
    EXPECT_TRUE(list.initialized());
    EXPECT_EQ(list.total_size(), 1);
    EXPECT_EQ(list.free_space(), 1);
    EXPECT_EQ(list.used_space(), 0);
    EXPECT_EQ(list.metadata_capacity(), 1);
    expect_single_free_range(list, 0, 1);

    auto initialized_again = list.init(allocator, 2, 2);
    ASSERT_FALSE(initialized_again);
    EXPECT_EQ(
        initialized_again.error(),
        free_list_error::already_initialized);
}

TEST(FreeList, ReportsMetadataAllocationFailure) {
    nk::mem::MallocAllocator parent{nk::mem::untracked};
    nk::mem::LinearAllocator constrained{
        nk::mem::untracked,
        parent,
        1};
    FreeList list;

    auto initialized = list.init(constrained, 64, 4);
    ASSERT_FALSE(initialized);
    EXPECT_EQ(initialized.error(), free_list_error::out_of_memory);
    EXPECT_FALSE(list.initialized());
    EXPECT_EQ(list.metadata_capacity(), 0);
}

TEST(FreeList, ReservesExactRangeAndRestoresIt) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 64, 1));

    auto reserved = list.reserve(64);
    ASSERT_TRUE(reserved);
    EXPECT_EQ(reserved->offset, 0);
    EXPECT_EQ(reserved->size, 64);
    EXPECT_EQ(list.range_count(), 0);
    EXPECT_EQ(list.free_space(), 0);
    EXPECT_EQ(list.used_space(), 64);

    ASSERT_TRUE(list.release(*reserved));
    EXPECT_EQ(list.free_space(), 64);
    EXPECT_EQ(list.used_space(), 0);
    expect_single_free_range(list, 0, 64);
}

TEST(FreeList, AlignsAndSplitsUsingFirstFit) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 64, 3));

    auto first = list.reserve(1);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->offset, 0);

    auto aligned = list.reserve(8, 8);
    ASSERT_TRUE(aligned);
    EXPECT_EQ(aligned->offset, 8);
    EXPECT_EQ(aligned->size, 8);
    ASSERT_EQ(list.range_count(), 2);
    ASSERT_NE(list.free_range(0), nullptr);
    EXPECT_EQ(list.free_range(0)->offset, 1);
    EXPECT_EQ(list.free_range(0)->size, 7);
    ASSERT_NE(list.free_range(1), nullptr);
    EXPECT_EQ(list.free_range(1)->offset, 16);
    EXPECT_EQ(list.free_range(1)->size, 48);
    EXPECT_EQ(list.free_space(), 55);
    EXPECT_EQ(list.used_space(), 9);

    auto first_fit = list.reserve(4, 4);
    ASSERT_TRUE(first_fit);
    EXPECT_EQ(first_fit->offset, 4);
}

TEST(FreeList, AppliesAlignmentBiasWithoutWastingThePrefix) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 64, 2));

    auto reserved = list.reserve(8, 16, 5);
    ASSERT_TRUE(reserved);
    EXPECT_EQ(reserved->offset, 11);
    EXPECT_EQ((reserved->offset + 5) % 16, 0);
    ASSERT_EQ(list.range_count(), 2);
    EXPECT_EQ(list.free_range(0)->offset, 0);
    EXPECT_EQ(list.free_range(0)->size, 11);
    EXPECT_EQ(list.free_range(1)->offset, 19);
    EXPECT_EQ(list.free_range(1)->size, 45);
}

TEST(FreeList, RejectsInvalidRequestsWithoutMutation) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 64, 1));

    auto zero = list.reserve(0);
    ASSERT_FALSE(zero);
    EXPECT_EQ(zero.error(), free_list_error::invalid_size);

    auto zero_alignment = list.reserve(1, 0);
    ASSERT_FALSE(zero_alignment);
    EXPECT_EQ(zero_alignment.error(), free_list_error::invalid_alignment);

    auto non_power_of_two = list.reserve(1, 3);
    ASSERT_FALSE(non_power_of_two);
    EXPECT_EQ(
        non_power_of_two.error(),
        free_list_error::invalid_alignment);

    auto too_large = list.reserve(65);
    ASSERT_FALSE(too_large);
    EXPECT_EQ(too_large.error(), free_list_error::out_of_space);

    EXPECT_EQ(list.free_space(), 64);
    EXPECT_EQ(list.used_space(), 0);
    expect_single_free_range(list, 0, 64);
}

TEST(FreeList, LeavesStateUntouchedWhenSplitNeedsMoreMetadata) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 64, 1));
    ASSERT_TRUE(list.reserve(1));
    expect_single_free_range(list, 1, 63);

    auto reserved = list.reserve(8, 8);
    ASSERT_FALSE(reserved);
    EXPECT_EQ(reserved.error(), free_list_error::metadata_exhausted);
    EXPECT_EQ(list.free_space(), 63);
    EXPECT_EQ(list.used_space(), 1);
    expect_single_free_range(list, 1, 63);
}

TEST(FreeList, UsesALaterRangeWhenTheFirstSplitNeedsMetadata) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 32, 2));
    ASSERT_TRUE(list.reserve(32));
    ASSERT_TRUE(list.release({1, 16}));
    ASSERT_TRUE(list.release({24, 8}));
    ASSERT_EQ(list.range_count(), 2);

    auto reserved = list.reserve(8, 8);
    ASSERT_TRUE(reserved);
    EXPECT_EQ(reserved->offset, 24);
    EXPECT_EQ(reserved->size, 8);
    expect_single_free_range(list, 1, 16);
}

TEST(FreeList, CoalescesReleasedRangesOnBothSides) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 100, 4));

    auto first = list.reserve(10);
    auto second = list.reserve(10);
    auto third = list.reserve(10);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(third);

    ASSERT_TRUE(list.release(*second));
    ASSERT_EQ(list.range_count(), 2);
    EXPECT_EQ(list.free_space(), 80);

    ASSERT_TRUE(list.release(*first));
    ASSERT_EQ(list.range_count(), 2);
    EXPECT_EQ(list.free_range(0)->offset, 0);
    EXPECT_EQ(list.free_range(0)->size, 20);

    ASSERT_TRUE(list.release(*third));
    EXPECT_EQ(list.free_space(), 100);
    EXPECT_EQ(list.used_space(), 0);
    expect_single_free_range(list, 0, 100);
}

TEST(FreeList, RejectsOverlappingAndOutOfBoundsReleases) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 64, 3));
    auto reserved = list.reserve(16);
    ASSERT_TRUE(reserved);

    ASSERT_TRUE(list.release({0, 8}));
    const nk::u64 free_before = list.free_space();
    const nk::u64 ranges_before = list.range_count();

    auto duplicate = list.release({0, 8});
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error(), free_list_error::overlapping_range);

    auto overlaps_tail = list.release({12, 8});
    ASSERT_FALSE(overlaps_tail);
    EXPECT_EQ(overlaps_tail.error(), free_list_error::overlapping_range);

    auto outside = list.release({60, 8});
    ASSERT_FALSE(outside);
    EXPECT_EQ(outside.error(), free_list_error::invalid_range);

    auto overflow = list.release({nk::numeric::u64_max - 2, 8});
    ASSERT_FALSE(overflow);
    EXPECT_EQ(overflow.error(), free_list_error::invalid_range);

    auto zero = list.release({8, 0});
    ASSERT_FALSE(zero);
    EXPECT_EQ(zero.error(), free_list_error::invalid_size);

    EXPECT_EQ(list.free_space(), free_before);
    EXPECT_EQ(list.range_count(), ranges_before);
}

TEST(FreeList, ReportsMetadataExhaustionOnSeparatedRelease) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 16, 1));
    auto allocated = list.reserve(16);
    ASSERT_TRUE(allocated);

    ASSERT_TRUE(list.release({0, 2}));
    auto separated = list.release({4, 2});
    ASSERT_FALSE(separated);
    EXPECT_EQ(separated.error(), free_list_error::metadata_exhausted);
    EXPECT_EQ(list.free_space(), 2);
    expect_single_free_range(list, 0, 2);
}

TEST(FreeList, GrowsAndShrinksOnlyThroughAFreeTail) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 100, 2));
    auto prefix = list.reserve(20);
    ASSERT_TRUE(prefix);

    ASSERT_TRUE(list.resize(120));
    EXPECT_EQ(list.total_size(), 120);
    EXPECT_EQ(list.free_space(), 100);
    expect_single_free_range(list, 20, 100);

    ASSERT_TRUE(list.resize(50));
    EXPECT_EQ(list.total_size(), 50);
    EXPECT_EQ(list.free_space(), 30);
    expect_single_free_range(list, 20, 30);

    ASSERT_TRUE(list.resize(20));
    EXPECT_EQ(list.total_size(), 20);
    EXPECT_EQ(list.free_space(), 0);
    EXPECT_EQ(list.range_count(), 0);

    auto blocked = list.resize(19);
    ASSERT_FALSE(blocked);
    EXPECT_EQ(blocked.error(), free_list_error::shrink_blocked);
    EXPECT_EQ(list.total_size(), 20);

    auto zero = list.resize(0);
    ASSERT_FALSE(zero);
    EXPECT_EQ(zero.error(), free_list_error::invalid_size);
}

TEST(FreeList, LeavesStateUntouchedWhenGrowthNeedsMoreMetadata) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 10, 1));
    ASSERT_TRUE(list.reserve(10));
    ASSERT_TRUE(list.release({0, 1}));

    auto grown = list.resize(20);
    ASSERT_FALSE(grown);
    EXPECT_EQ(grown.error(), free_list_error::metadata_exhausted);
    EXPECT_EQ(list.total_size(), 10);
    EXPECT_EQ(list.free_space(), 1);
    expect_single_free_range(list, 0, 1);
}

TEST(FreeList, ClearRestoresTheEntireAddressSpace) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 128, 8));
    ASSERT_TRUE(list.reserve(11, 4));
    ASSERT_TRUE(list.reserve(23, 16));

    ASSERT_TRUE(list.clear());
    EXPECT_EQ(list.total_size(), 128);
    EXPECT_EQ(list.free_space(), 128);
    EXPECT_EQ(list.used_space(), 0);
    expect_single_free_range(list, 0, 128);
}

TEST(FreeList, PerformsNoMetadataAllocationsAfterInitialization) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, 128, 16));
    ASSERT_EQ(allocator.get_active_allocation_count(), 1);
    const nk::u64 peak = allocator.get_peak_used_bytes();

    auto first = list.reserve(7, 8);
    auto second = list.reserve(13, 16);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(list.release(*first));
    ASSERT_TRUE(list.release(*second));
    ASSERT_TRUE(list.resize(256));
    ASSERT_TRUE(list.resize(128));
    ASSERT_TRUE(list.clear());

    EXPECT_EQ(allocator.get_active_allocation_count(), 1);
    EXPECT_EQ(allocator.get_peak_used_bytes(), peak);
    list.shutdown();
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(FreeList, MoveOperationsTransferMetadataAndState) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList source;
    ASSERT_TRUE(source.init(allocator, 64, 4));
    ASSERT_TRUE(source.reserve(16));

    FreeList moved{std::move(source)};
    EXPECT_FALSE(source.initialized());
    EXPECT_EQ(source.metadata_capacity(), 0);
    EXPECT_TRUE(moved.initialized());
    EXPECT_EQ(moved.total_size(), 64);
    EXPECT_EQ(moved.used_space(), 16);

    FreeList destination;
    ASSERT_TRUE(destination.init(allocator, 32, 2));
    destination = std::move(moved);
    EXPECT_FALSE(moved.initialized());
    EXPECT_EQ(moved.metadata_capacity(), 0);
    EXPECT_TRUE(destination.initialized());
    EXPECT_EQ(destination.total_size(), 64);
    EXPECT_EQ(destination.used_space(), 16);
    EXPECT_EQ(allocator.get_active_allocation_count(), 1);

    ASSERT_TRUE(destination.release({0, 16}));
    expect_single_free_range(destination, 0, 64);
}

TEST(FreeList, MatchesAByteModelAcrossDeterministicOperations) {
    constexpr nk::u64 total_size = 128;
    constexpr nk::u64 maximum_live = 64;
    struct LiveRange {
        MemoryRange range{};
        bool active = false;
    };

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    FreeList list;
    ASSERT_TRUE(list.init(allocator, total_size, total_size));

    bool occupied[total_size]{};
    LiveRange live[maximum_live]{};
    nk::u64 live_count = 0;
    nk::u64 state = 0x9e3779b97f4a7c15ULL;

    for (nk::u64 step = 0; step < 2'000; ++step) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        const bool should_allocate =
            live_count == 0 ||
            (live_count < maximum_live && (state & 1ULL) != 0);

        if (should_allocate) {
            const nk::u64 size = ((state >> 8) % 8) + 1;
            const nk::u64 alignment = 1ULL << ((state >> 16) % 4);
            nk::u64 expected_offset = nk::numeric::u64_max;
            for (nk::u64 offset = 0; offset + size <= total_size; ++offset) {
                if ((offset & (alignment - 1)) != 0)
                    continue;
                bool available = true;
                for (nk::u64 byte = 0; byte < size; ++byte) {
                    if (occupied[offset + byte]) {
                        available = false;
                        break;
                    }
                }
                if (available) {
                    expected_offset = offset;
                    break;
                }
            }

            auto reserved = list.reserve(size, alignment);
            if (expected_offset == nk::numeric::u64_max) {
                ASSERT_FALSE(reserved);
                EXPECT_EQ(reserved.error(), free_list_error::out_of_space);
            } else {
                ASSERT_TRUE(reserved);
                EXPECT_EQ(reserved->offset, expected_offset);
                for (nk::u64 byte = 0; byte < size; ++byte)
                    occupied[expected_offset + byte] = true;

                for (LiveRange& entry : live) {
                    if (!entry.active) {
                        entry.range = *reserved;
                        entry.active = true;
                        ++live_count;
                        break;
                    }
                }
            }
        } else {
            const nk::u64 selection = (state >> 24) % live_count;
            nk::u64 current = 0;
            for (LiveRange& entry : live) {
                if (!entry.active)
                    continue;
                if (current++ != selection)
                    continue;

                ASSERT_TRUE(list.release(entry.range));
                for (nk::u64 byte = 0; byte < entry.range.size; ++byte)
                    occupied[entry.range.offset + byte] = false;
                entry.active = false;
                --live_count;
                break;
            }
        }

        nk::u64 occupied_count = 0;
        for (const bool byte : occupied) {
            if (byte)
                ++occupied_count;
        }
        EXPECT_EQ(list.used_space(), occupied_count);
        EXPECT_EQ(list.free_space(), total_size - occupied_count);
    }

    for (LiveRange& entry : live) {
        if (entry.active) {
            ASSERT_TRUE(list.release(entry.range));
        }
    }
    expect_single_free_range(list, 0, total_size);
}
