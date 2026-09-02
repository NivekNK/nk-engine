#include <gtest/gtest.h>

#include "systems/memory_system.h"
#include "memory/malloc_allocator.h"

TEST(MallocAllocator, HonorsSupportedAlignmentsAndTracksSuccessfulOperations) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    constexpr nk::u64 allocation_size = 37;
    constexpr nk::u64 alignments[] = {1, 2, 4, 8, 16, 32, 64, 256, 4096};

    for (const nk::u64 alignment : alignments) {
        void* data = allocator._allocate_raw(allocation_size, alignment);
        ASSERT_NE(data, nullptr) << "alignment=" << alignment;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(data) % alignment, 0)
            << "alignment=" << alignment;
        EXPECT_EQ(allocator.get_reserved_bytes(), allocation_size);
        EXPECT_EQ(allocator.get_used_bytes(), allocation_size);
        EXPECT_EQ(allocator.get_active_allocation_count(), 1);

        std::memset(data, 0xa5, allocation_size);
        EXPECT_TRUE(allocator._free_raw(data, allocation_size));
        EXPECT_EQ(allocator.get_reserved_bytes(), 0);
        EXPECT_EQ(allocator.get_used_bytes(), 0);
        EXPECT_EQ(allocator.get_active_allocation_count(), 0);
    }

    EXPECT_EQ(allocator.get_peak_used_bytes(), allocation_size);
}

TEST(MallocAllocator, KeepsRawAndZeroedAllocationExplicit) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    constexpr nk::u64 byte_count = 257;
    auto* data = static_cast<nk::u8*>(allocator._allocate_zeroed_raw(byte_count, 256));
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(data) % 256, 0);
    for (nk::u64 index = 0; index < byte_count; ++index)
        EXPECT_EQ(data[index], 0);
    EXPECT_TRUE(allocator._free_raw(data, byte_count));

    nk::u32* values = allocator._allocate_zeroed_lot_t<nk::u32>(16);
    ASSERT_NE(values, nullptr);
    for (nk::u32 index = 0; index < 16; ++index)
        EXPECT_EQ(values[index], 0u);
    EXPECT_TRUE(allocator._free_lot_t(values, 16));
}

TEST(MallocAllocator, RejectsInvalidOperationsWithoutChangingStatistics) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    EXPECT_EQ(allocator._allocate_raw(0, alignof(std::max_align_t)), nullptr);
    EXPECT_EQ(allocator._allocate_raw(32, 0), nullptr);
    EXPECT_EQ(allocator._allocate_raw(32, 3), nullptr);
    EXPECT_FALSE(allocator._free_raw(nullptr, 32));
    EXPECT_FALSE(allocator._free_raw(reinterpret_cast<void*>(1), 0));

    EXPECT_EQ(allocator.get_reserved_bytes(), 0);
    EXPECT_EQ(allocator.get_used_bytes(), 0);
    EXPECT_EQ(allocator.get_peak_used_bytes(), 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

#if NK_MEMORY_TRACKING_ENABLED
TEST(MallocAllocator, DetectsSizeMismatchUnknownAddressAndDoubleFree) {
    auto& tracker = nk::mem::MemorySystem::init();
    ASSERT_EQ(tracker.state(), nk::mem::MemorySystemState::Ready);

    {
        nk::mem::MallocAllocator allocator;
        ASSERT_NE(
            allocator._allocator_init_tracked<nk::mem::MallocAllocator>(
                tracker,
                __FILE__,
                __LINE__,
                "Validated malloc allocator",
                nk::MemoryType::Test),
            nullptr);

        constexpr nk::u64 size_bytes = 64;
        void* data = allocator._allocate_raw(size_bytes, 64);
        ASSERT_NE(data, nullptr);

        EXPECT_FALSE(allocator._free_raw(data, size_bytes / 2));
        EXPECT_FALSE(allocator._free_raw(reinterpret_cast<void*>(1), size_bytes));
        EXPECT_EQ(allocator.get_reserved_bytes(), size_bytes);
        EXPECT_EQ(allocator.get_used_bytes(), size_bytes);
        EXPECT_EQ(allocator.get_active_allocation_count(), 1);

        EXPECT_TRUE(allocator._free_raw(data, size_bytes));
        EXPECT_FALSE(allocator._free_raw(data, size_bytes));
        EXPECT_EQ(allocator.get_reserved_bytes(), 0);
        EXPECT_EQ(allocator.get_used_bytes(), 0);
        EXPECT_EQ(allocator.get_active_allocation_count(), 0);
    }

    nk::mem::MemorySystem::shutdown();
}
#endif

TEST(MallocAllocator, SupportsNativeAndTypedAllocationHelpers) {
    NK_MEMORY_SYSTEM_INIT();

    nk::u32* data = native_allocate_lot(nk::u32, 5);

    {
        nk::mem::MallocAllocator allocator;
        allocator.allocator_init(nk::mem::MallocAllocator, "Test", nk::MemoryType::System);

        constexpr nk::u8 lot = 10;
        nk::u32* lot_data = allocator.allocate_lot_t(nk::u32, lot);

        allocator.free_lot_t(nk::u32, lot_data, lot);
    }
    native_free_lot(nk::u32, data, 5);

    NK_MEMORY_SYSTEM_SHUTDOWN();
}
