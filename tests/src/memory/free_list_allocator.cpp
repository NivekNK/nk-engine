#include <gtest/gtest.h>

#include "memory/allocator_owner.h"
#include "memory/free_list_allocator.h"
#include "memory/linear_allocator.h"
#include "memory/malloc_allocator.h"
#include "systems/memory_system.h"

TEST(FreeListAllocator, OwnsOrBorrowsBackingStorageExplicitly) {
    nk::mem::MallocAllocator backing{nk::mem::untracked};
    nk::mem::MallocAllocator metadata{nk::mem::untracked};

    {
        nk::mem::FreeListAllocator owned{
            nk::mem::untracked,
            backing,
            metadata,
            512,
            8};
        ASSERT_TRUE(owned.is_initialized());
        EXPECT_TRUE(owned.owns_backing_memory());
        EXPECT_EQ(owned.backing_allocator(), &backing);
        EXPECT_EQ(owned.metadata_allocator(), &metadata);
        EXPECT_EQ(owned.get_reserved_bytes(), 512);
        EXPECT_EQ(owned.free_space(), 512);
        EXPECT_EQ(backing.get_active_allocation_count(), 1);
        EXPECT_EQ(metadata.get_active_allocation_count(), 1);
    }

    EXPECT_EQ(backing.get_active_allocation_count(), 0);
    EXPECT_EQ(metadata.get_active_allocation_count(), 0);

    alignas(64) nk::u8 external_storage[256]{};
    {
        nk::mem::FreeListAllocator borrowed{
            nk::mem::untracked,
            metadata,
            external_storage,
            sizeof(external_storage),
            4};
        ASSERT_TRUE(borrowed.is_initialized());
        EXPECT_FALSE(borrowed.owns_backing_memory());
        EXPECT_EQ(borrowed.backing_allocator(), nullptr);
        EXPECT_EQ(borrowed.get_data(), external_storage);
        EXPECT_EQ(metadata.get_active_allocation_count(), 1);
    }

    external_storage[0] = 42;
    EXPECT_EQ(external_storage[0], 42);
    EXPECT_EQ(metadata.get_active_allocation_count(), 0);
}

TEST(FreeListAllocator, RejectsInvalidConfigurationsAndRollsBack) {
    nk::mem::MallocAllocator backing{nk::mem::untracked};
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    alignas(64) nk::u8 storage[256]{};

    nk::mem::FreeListAllocator zero_capacity;
    EXPECT_EQ(
        zero_capacity._allocator_init_untracked<nk::mem::FreeListAllocator>(
            metadata,
            storage,
            sizeof(storage),
            0),
        nullptr);

    nk::mem::FreeListAllocator null_storage;
    EXPECT_EQ(
        null_storage._allocator_init_untracked<nk::mem::FreeListAllocator>(
            metadata,
            nullptr,
            sizeof(storage),
            4),
        nullptr);

    nk::mem::FreeListAllocator undersized;
    EXPECT_EQ(
        undersized._allocator_init_untracked<nk::mem::FreeListAllocator>(
            metadata,
            storage,
            nk::mem::FreeListAllocator::allocation_overhead,
            4),
        nullptr);

    nk::mem::FreeListAllocator circular_metadata;
    EXPECT_EQ(
        circular_metadata._allocator_init_untracked<
            nk::mem::FreeListAllocator>(
                circular_metadata,
                storage,
                sizeof(storage),
                4),
        nullptr);

    nk::mem::MallocAllocator unavailable_backing;
    nk::mem::FreeListAllocator unavailable;
    EXPECT_EQ(
        unavailable._allocator_init_untracked<nk::mem::FreeListAllocator>(
            unavailable_backing,
            metadata,
            sizeof(storage),
            4),
        nullptr);

    nk::mem::LinearAllocator constrained_metadata{
        nk::mem::untracked,
        backing,
        1};
    const nk::u64 active_before = backing.get_active_allocation_count();
    nk::mem::FreeListAllocator metadata_failure;
    EXPECT_EQ(
        metadata_failure._allocator_init_untracked<
            nk::mem::FreeListAllocator>(
                backing,
                constrained_metadata,
                sizeof(storage),
                4),
        nullptr);
    EXPECT_EQ(backing.get_active_allocation_count(), active_before);
}

TEST(FreeListAllocator, AlignsPointersFromAMisalignedBackingStore) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    alignas(256) nk::u8 storage[2049]{};
    void* external_data = storage + 1;
    constexpr nk::u64 capacity = 2048;
    nk::mem::FreeListAllocator allocator{
        nk::mem::untracked,
        metadata,
        external_data,
        capacity,
        16};
    ASSERT_TRUE(allocator.is_initialized());

    constexpr nk::u64 sizes[]{3, 5, 17, 33};
    constexpr nk::u64 alignments[]{1, 16, 64, 256};
    void* allocations[4]{};
    nk::u64 payload_bytes = 0;
    for (nk::u64 index = 0; index < 4; ++index) {
        allocations[index] = allocator._allocate_raw(
            sizes[index],
            alignments[index]);
        ASSERT_NE(allocations[index], nullptr);
        EXPECT_EQ(
            reinterpret_cast<std::uintptr_t>(allocations[index]) %
                alignments[index],
            0);
        payload_bytes += sizes[index];
        EXPECT_EQ(allocator.get_used_bytes(), payload_bytes);
        EXPECT_EQ(allocator.get_active_allocation_count(), index + 1);
    }

    EXPECT_EQ(
        allocator.occupied_space(),
        payload_bytes +
            4 * nk::mem::FreeListAllocator::allocation_overhead);
    EXPECT_EQ(
        allocator.free_space() + allocator.occupied_space(),
        capacity);

    constexpr nk::u64 release_order[]{2, 0, 3, 1};
    for (const nk::u64 index : release_order)
        ASSERT_TRUE(allocator._free_raw(allocations[index], sizes[index]));

    EXPECT_EQ(allocator.free_space(), capacity);
    EXPECT_EQ(allocator.get_used_bytes(), 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(FreeListAllocator, RoundTripsEveryPowerOfTwoAlignmentThrough4096) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    alignas(4096) nk::u8 storage[16385]{};
    nk::mem::FreeListAllocator allocator{
        nk::mem::untracked,
        metadata,
        storage + 1,
        sizeof(storage) - 1,
        4};
    ASSERT_TRUE(allocator.is_initialized());

    for (nk::u64 alignment = 1; alignment <= 4096; alignment <<= 1) {
        void* data = allocator._allocate_raw(13, alignment);
        ASSERT_NE(data, nullptr) << "alignment=" << alignment;
        EXPECT_EQ(
            reinterpret_cast<std::uintptr_t>(data) % alignment,
            0u);
        ASSERT_TRUE(allocator._free_raw(data, 13));
        EXPECT_EQ(allocator.free_space(), sizeof(storage) - 1);
    }
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(FreeListAllocator, ReusesReleasedBlocks) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    alignas(64) nk::u8 storage[512]{};
    nk::mem::FreeListAllocator allocator{
        nk::mem::untracked,
        metadata,
        storage,
        sizeof(storage),
        8};

    void* first = allocator._allocate_raw(64, 64);
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(allocator._free_raw(first, 64));

    void* reused = allocator._allocate_raw(64, 64);
    ASSERT_NE(reused, nullptr);
    EXPECT_EQ(reused, first);
    EXPECT_TRUE(allocator._free_raw(reused, 64));
}

TEST(FreeListAllocator, ReportsFragmentationWithoutMutatingState) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    alignas(64) nk::u8 storage[280]{};
    nk::mem::FreeListAllocator allocator{
        nk::mem::untracked,
        metadata,
        storage,
        sizeof(storage),
        8};

    void* first = allocator._allocate_raw(64, 1);
    void* middle = allocator._allocate_raw(64, 1);
    void* last = allocator._allocate_raw(64, 1);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(middle, nullptr);
    ASSERT_NE(last, nullptr);
    ASSERT_TRUE(allocator._free_raw(middle, 64));
    ASSERT_EQ(allocator.free_space(), 136);

    const auto before = allocator.statistics();
    EXPECT_EQ(allocator._allocate_raw(100, 1), nullptr);
    EXPECT_EQ(allocator.free_space(), 136);
    EXPECT_EQ(allocator.get_used_bytes(), before.used_bytes);
    EXPECT_EQ(
        allocator.get_active_allocation_count(),
        before.active_allocations);

    ASSERT_TRUE(allocator._free_raw(last, 64));
    void* coalesced = allocator._allocate_raw(100, 1);
    ASSERT_NE(coalesced, nullptr);
    ASSERT_TRUE(allocator._free_raw(coalesced, 100));
    ASSERT_TRUE(allocator._free_raw(first, 64));
    EXPECT_EQ(allocator.free_space(), sizeof(storage));
}

TEST(FreeListAllocator, RejectsInvalidFreesWithoutTracking) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    alignas(64) nk::u8 storage[512]{};
    nk::mem::FreeListAllocator allocator{
        nk::mem::untracked,
        metadata,
        storage,
        sizeof(storage),
        8};
    void* data = allocator._allocate_raw(64, 32);
    ASSERT_NE(data, nullptr);
    const auto allocated = allocator.statistics();
    const nk::u64 free_before = allocator.free_space();

    EXPECT_FALSE(allocator._free_raw(data, 32));
    EXPECT_FALSE(allocator._free_raw(static_cast<nk::u8*>(data) + 1, 64));
    EXPECT_FALSE(allocator._free_raw(storage, 64));
    int foreign = 0;
    EXPECT_FALSE(allocator._free_raw(&foreign, sizeof(foreign)));
    EXPECT_FALSE(allocator._free_raw(data, 0));

    EXPECT_EQ(allocator.free_space(), free_before);
    EXPECT_EQ(allocator.get_used_bytes(), allocated.used_bytes);
    EXPECT_EQ(
        allocator.get_active_allocation_count(),
        allocated.active_allocations);

    ASSERT_TRUE(allocator._free_raw(data, 64));
    EXPECT_FALSE(allocator._free_raw(data, 64));
    EXPECT_EQ(allocator.free_space(), sizeof(storage));
}

TEST(FreeListAllocator, RejectsOverflowAndOutOfSpaceTransactionally) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    alignas(64) nk::u8 storage[128]{};
    nk::mem::FreeListAllocator allocator{
        nk::mem::untracked,
        metadata,
        storage,
        sizeof(storage),
        4};

    EXPECT_EQ(
        allocator._allocate_raw(nk::numeric::u64_max, 1),
        nullptr);
    EXPECT_EQ(allocator._allocate_raw(121, 1), nullptr);
    EXPECT_EQ(allocator.free_space(), sizeof(storage));
    EXPECT_EQ(allocator.get_used_bytes(), 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);

    void* maximum = allocator._allocate_raw(120, 1);
    ASSERT_NE(maximum, nullptr);
    EXPECT_EQ(allocator.free_space(), 0);
    EXPECT_TRUE(allocator._free_raw(maximum, 120));
}

TEST(FreeListAllocator, DoesNotAllocateMetadataAfterInitialization) {
    nk::mem::MallocAllocator parent{nk::mem::untracked};
    nk::mem::FreeListAllocator allocator{
        nk::mem::untracked,
        parent,
        parent,
        1024,
        16};
    ASSERT_TRUE(allocator.is_initialized());
    ASSERT_EQ(parent.get_active_allocation_count(), 2);
    const nk::u64 parent_peak = parent.get_peak_used_bytes();

    for (nk::u64 index = 0; index < 100; ++index) {
        void* data = allocator._allocate_raw(32, 64);
        ASSERT_NE(data, nullptr);
        ASSERT_TRUE(allocator._free_raw(data, 32));
    }

    EXPECT_EQ(parent.get_active_allocation_count(), 2);
    EXPECT_EQ(parent.get_peak_used_bytes(), parent_peak);
    EXPECT_EQ(allocator.free_space(), 1024);
}

TEST(FreeListAllocator, MoveConstructionTransfersLiveAllocations) {
    nk::mem::MallocAllocator backing{nk::mem::untracked};
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    nk::mem::FreeListAllocator source{
        nk::mem::untracked,
        backing,
        metadata,
        512,
        8};
    void* data = source._allocate_raw(48, 64);
    ASSERT_NE(data, nullptr);

    nk::mem::FreeListAllocator destination{std::move(source)};
    EXPECT_EQ(source.lifecycle(), nk::mem::AllocatorLifecycle::MovedFrom);
    EXPECT_EQ(source.get_data(), nullptr);
    EXPECT_EQ(source.metadata_capacity(), 0);
    EXPECT_TRUE(destination.owns_backing_memory());
    EXPECT_EQ(destination.get_active_allocation_count(), 1);
    EXPECT_TRUE(destination._free_raw(data, 48));
}

TEST(FreeListAllocator, MoveAssignmentReleasesOnlyAnEmptyDestination) {
    nk::mem::MallocAllocator backing{nk::mem::untracked};
    nk::mem::MallocAllocator metadata{nk::mem::untracked};

    nk::mem::FreeListAllocator source{
        nk::mem::untracked,
        backing,
        metadata,
        512,
        8};
    nk::mem::FreeListAllocator destination{
        nk::mem::untracked,
        backing,
        metadata,
        256,
        4};
    void* source_data = source._allocate_raw(32, 32);
    ASSERT_NE(source_data, nullptr);
    ASSERT_EQ(backing.get_active_allocation_count(), 2);
    ASSERT_EQ(metadata.get_active_allocation_count(), 2);

    destination = std::move(source);
    EXPECT_EQ(source.lifecycle(), nk::mem::AllocatorLifecycle::MovedFrom);
    EXPECT_EQ(backing.get_active_allocation_count(), 1);
    EXPECT_EQ(metadata.get_active_allocation_count(), 1);
    EXPECT_EQ(destination.get_active_allocation_count(), 1);
    EXPECT_TRUE(destination._free_raw(source_data, 32));

    nk::mem::FreeListAllocator occupied_source{
        nk::mem::untracked,
        backing,
        metadata,
        256,
        4};
    nk::mem::FreeListAllocator occupied_destination{
        nk::mem::untracked,
        backing,
        metadata,
        256,
        4};
    void* first = occupied_source._allocate_raw(16, 8);
    void* second = occupied_destination._allocate_raw(16, 8);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    void* destination_backing = occupied_destination.get_data();

    occupied_destination = std::move(occupied_source);
    EXPECT_EQ(occupied_destination.get_data(), destination_backing);
    EXPECT_EQ(
        occupied_source.lifecycle(),
        nk::mem::AllocatorLifecycle::Initialized);
    EXPECT_TRUE(occupied_source._free_raw(first, 16));
    EXPECT_TRUE(occupied_destination._free_raw(second, 16));
}

TEST(FreeListAllocator, WorksThroughAllocatorOwner) {
    static_assert(
        std::is_nothrow_destructible_v<nk::mem::FreeListAllocator>);
    nk::mem::MallocAllocator backing{nk::mem::untracked};
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    auto owner =
        nk::mem::AllocatorOwner::make_native_untracked<
            nk::mem::FreeListAllocator>(
                nk::mem::untracked,
                backing,
                metadata,
                512,
                8);
    ASSERT_TRUE(owner);
    auto* allocator = static_cast<nk::mem::FreeListAllocator*>(owner.get());
    ASSERT_TRUE(allocator->is_initialized());
    void* data = allocator->_allocate_raw(64, 64);
    ASSERT_NE(data, nullptr);
    ASSERT_TRUE(allocator->_free_raw(data, 64));

    owner.reset();
    EXPECT_EQ(backing.get_active_allocation_count(), 0);
    EXPECT_EQ(metadata.get_active_allocation_count(), 0);
}

#if NK_MEMORY_TRACKING_ENABLED
TEST(FreeListAllocator, ReportsBalancedTrackedAllocations) {
    auto& tracker = nk::mem::MemorySystem::init();
    ASSERT_EQ(tracker.state(), nk::mem::MemorySystemState::Ready);
    const nk::u64 events_before = tracker.allocation_event_count();

    {
        nk::mem::MallocAllocator parent;
        ASSERT_NE(
            parent.allocator_init(
                nk::mem::MallocAllocator,
                "Free-list parent",
                nk::MemoryType::Test),
            nullptr);

        nk::mem::FreeListAllocator allocator;
        ASSERT_NE(
            allocator.allocator_init(
                nk::mem::FreeListAllocator,
                "Reusable test pool",
                nk::MemoryType::Test,
                parent,
                parent,
                1024,
                16),
            nullptr);
        EXPECT_TRUE(allocator.is_tracked());
        EXPECT_STREQ(allocator._allocator_name(), "Reusable test pool");

        void* first = allocator.allocate_raw(80, 64);
        void* second = allocator.allocate_zeroed_raw(120, 256);
        ASSERT_NE(first, nullptr);
        ASSERT_NE(second, nullptr);
        EXPECT_EQ(allocator.get_used_bytes(), 200);
        EXPECT_EQ(allocator.get_active_allocation_count(), 2);
        EXPECT_TRUE(allocator.free_raw(second, 120));
        EXPECT_TRUE(allocator.free_raw(first, 80));
        EXPECT_EQ(allocator.get_used_bytes(), 0);
        EXPECT_EQ(allocator.get_active_allocation_count(), 0);
    }

    EXPECT_GE(tracker.allocation_event_count(), events_before + 4);
    nk::mem::MemorySystem::shutdown();
}
#endif
