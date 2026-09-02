#include <gtest/gtest.h>

#include "memory/linear_allocator.h"
#include "memory/malloc_allocator.h"
#include "systems/memory_system.h"

namespace {
    nk::u64 next_aligned_offset(
        const void* base,
        nk::u64 offset,
        nk::u64 alignment) {
        const nk::u64 address = static_cast<nk::u64>(
            reinterpret_cast<std::uintptr_t>(base)) + offset;
        const nk::u64 misalignment = address & (alignment - 1);
        return offset + (misalignment == 0 ? 0 : alignment - misalignment);
    }
}

TEST(LinearAllocator, AlignsMixedAllocationsAndCountsPadding) {
    alignas(64) nk::u8 storage[768]{};
    void* external_data = storage + 1;
    constexpr nk::u64 capacity = 640;

    nk::mem::LinearAllocator allocator;
    ASSERT_NE(
        allocator._allocator_init_untracked<nk::mem::LinearAllocator>(
            capacity,
            external_data),
        nullptr);

    constexpr nk::u64 sizes[] = {3, 5, 17, 9, 33};
    constexpr nk::u64 alignments[] = {1, 16, 64, 256, 32};
    nk::u64 expected_offset = 0;
    std::uintptr_t previous_end = 0;

    for (nk::u64 index = 0; index < 5; ++index) {
        const nk::u64 aligned_offset = next_aligned_offset(
            external_data,
            expected_offset,
            alignments[index]);
        void* data = allocator._allocate_raw(sizes[index], alignments[index]);
        ASSERT_NE(data, nullptr) << "allocation=" << index;
        EXPECT_EQ(
            reinterpret_cast<std::uintptr_t>(data) % alignments[index],
            0) << "allocation=" << index;
        EXPECT_EQ(
            data,
            static_cast<nk::u8*>(external_data) + aligned_offset);

        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(data);
        if (index != 0) {
            EXPECT_LE(previous_end, address);
        }
        previous_end = address + sizes[index];
        expected_offset = aligned_offset + sizes[index];

        EXPECT_EQ(allocator.get_used_bytes(), expected_offset);
        EXPECT_EQ(allocator.get_active_allocation_count(), index + 1);
    }

    EXPECT_EQ(allocator.get_reserved_bytes(), capacity);
    EXPECT_EQ(allocator.get_peak_used_bytes(), expected_offset);

    const auto statistics_before_failure = allocator.statistics();
    EXPECT_EQ(
        allocator._allocate_raw(nk::numeric::u64_max, 64),
        nullptr);
    EXPECT_EQ(allocator.get_used_bytes(), statistics_before_failure.used_bytes);
    EXPECT_EQ(
        allocator.get_active_allocation_count(),
        statistics_before_failure.active_allocations);

    EXPECT_EQ(allocator._allocate_raw(capacity, 256), nullptr);
    EXPECT_EQ(allocator.get_used_bytes(), statistics_before_failure.used_bytes);
    EXPECT_EQ(
        allocator.get_active_allocation_count(),
        statistics_before_failure.active_allocations);

    EXPECT_TRUE(allocator.reset(nk::mem::LinearResetMode::RetainContents));
    EXPECT_EQ(allocator.get_reserved_bytes(), capacity);
    EXPECT_EQ(allocator.get_used_bytes(), 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
    EXPECT_EQ(allocator.get_peak_used_bytes(), expected_offset);
}

TEST(LinearAllocator, RejectsIndividualFreeWithoutChangingCursor) {
    alignas(64) nk::u8 storage[256]{};
    nk::mem::LinearAllocator allocator{
        nk::mem::untracked,
        sizeof(storage),
        storage};

    void* data = allocator._allocate_raw(32, 32);
    ASSERT_NE(data, nullptr);
    const auto statistics = allocator.statistics();

    EXPECT_FALSE(allocator._free_raw(data, 32));
    EXPECT_EQ(allocator.get_reserved_bytes(), statistics.reserved_bytes);
    EXPECT_EQ(allocator.get_used_bytes(), statistics.used_bytes);
    EXPECT_EQ(allocator.get_peak_used_bytes(), statistics.peak_used_bytes);
    EXPECT_EQ(
        allocator.get_active_allocation_count(),
        statistics.active_allocations);

    EXPECT_TRUE(allocator.reset(nk::mem::LinearResetMode::RetainContents));
}

TEST(LinearAllocator, AppliesBuildDefaultAndExplicitZeroResetPolicies) {
    alignas(64) nk::u8 storage[128];
    for (nk::u8& byte : storage)
        byte = 0xa5;

    nk::mem::LinearAllocator allocator{
        nk::mem::untracked,
        sizeof(storage),
        storage};
    ASSERT_NE(allocator._allocate_raw(32, 1), nullptr);
    ASSERT_TRUE(allocator.reset());

#if NK_MEMORY_TRACKING_ENABLED
    for (const nk::u8 byte : storage)
        EXPECT_EQ(byte, 0);
#else
    for (const nk::u8 byte : storage)
        EXPECT_EQ(byte, 0xa5);
#endif

    for (nk::u8& byte : storage)
        byte = 0x5a;
    ASSERT_TRUE(allocator.reset(nk::mem::LinearResetMode::ZeroMemory));
    for (const nk::u8 byte : storage)
        EXPECT_EQ(byte, 0);
}

TEST(LinearAllocator, DistinguishesOwnedAndExternalBackingMemory) {
    nk::mem::MallocAllocator parent{nk::mem::untracked};
    constexpr nk::u64 capacity = 512;

    {
        nk::mem::LinearAllocator owned{
            nk::mem::untracked,
            parent,
            capacity};
        EXPECT_TRUE(owned.is_initialized());
        EXPECT_TRUE(owned.owns_backing_memory());
        EXPECT_EQ(owned.backing_allocator(), &parent);
        EXPECT_NE(owned.get_data(), nullptr);
        EXPECT_EQ(parent.get_reserved_bytes(), capacity);
        EXPECT_EQ(parent.get_active_allocation_count(), 1);

        ASSERT_NE(owned._allocate_raw(64, 64), nullptr);
        EXPECT_TRUE(owned.reset(nk::mem::LinearResetMode::RetainContents));
    }

    EXPECT_EQ(parent.get_reserved_bytes(), 0);
    EXPECT_EQ(parent.get_active_allocation_count(), 0);

    alignas(64) nk::u8 external_storage[128]{};
    {
        nk::mem::LinearAllocator external{
            nk::mem::untracked,
            sizeof(external_storage),
            external_storage};
        EXPECT_FALSE(external.owns_backing_memory());
        EXPECT_EQ(external.backing_allocator(), nullptr);
        ASSERT_NE(external._allocate_raw(16, 16), nullptr);
        EXPECT_TRUE(external.reset(nk::mem::LinearResetMode::RetainContents));
    }
    external_storage[0] = 42;
    EXPECT_EQ(external_storage[0], 42);
}

TEST(LinearAllocator, RejectsInvalidOrUnavailableBackingMemory) {
    alignas(16) nk::u8 storage[64]{};

    nk::mem::LinearAllocator null_external;
    EXPECT_EQ(
        null_external._allocator_init_untracked<nk::mem::LinearAllocator>(
            sizeof(storage),
            nullptr),
        nullptr);
    EXPECT_EQ(
        null_external.lifecycle(),
        nk::mem::AllocatorLifecycle::InitializationFailed);

    nk::mem::LinearAllocator zero_capacity;
    EXPECT_EQ(
        zero_capacity._allocator_init_untracked<nk::mem::LinearAllocator>(
            0,
            storage),
        nullptr);
    EXPECT_EQ(
        zero_capacity.lifecycle(),
        nk::mem::AllocatorLifecycle::InitializationFailed);

    nk::mem::MallocAllocator unavailable_parent;
    nk::mem::LinearAllocator missing_owned_backing;
    EXPECT_EQ(
        missing_owned_backing._allocator_init_untracked<nk::mem::LinearAllocator>(
            unavailable_parent,
            sizeof(storage)),
        nullptr);
    EXPECT_EQ(
        missing_owned_backing.lifecycle(),
        nk::mem::AllocatorLifecycle::InitializationFailed);
}

TEST(LinearAllocator, MoveConstructionTransfersOwnedBackingAndCursor) {
    nk::mem::MallocAllocator parent{nk::mem::untracked};
    constexpr nk::u64 capacity = 256;

    {
        nk::mem::LinearAllocator source{
            nk::mem::untracked,
            parent,
            capacity};
        void* source_backing = source.get_data();
        ASSERT_NE(source._allocate_raw(33, 64), nullptr);
        const nk::u64 source_used = source.get_used_bytes();

        nk::mem::LinearAllocator destination{std::move(source)};
        EXPECT_EQ(source.lifecycle(), nk::mem::AllocatorLifecycle::MovedFrom);
        EXPECT_FALSE(source.owns_backing_memory());
        EXPECT_EQ(destination.get_data(), source_backing);
        EXPECT_EQ(destination.get_used_bytes(), source_used);
        EXPECT_TRUE(destination.owns_backing_memory());
        EXPECT_EQ(destination.backing_allocator(), &parent);
        EXPECT_EQ(parent.get_active_allocation_count(), 1);

        EXPECT_TRUE(destination.reset(nk::mem::LinearResetMode::RetainContents));
    }

    EXPECT_EQ(parent.get_active_allocation_count(), 0);
}

TEST(LinearAllocator, MoveAssignmentReleasesEmptyDestinationBacking) {
    nk::mem::MallocAllocator parent{nk::mem::untracked};

    {
        nk::mem::LinearAllocator source{
            nk::mem::untracked,
            parent,
            256};
        nk::mem::LinearAllocator destination{
            nk::mem::untracked,
            parent,
            128};
        void* source_backing = source.get_data();
        ASSERT_NE(source._allocate_raw(24, 32), nullptr);
        EXPECT_EQ(parent.get_active_allocation_count(), 2);

        destination = std::move(source);
        EXPECT_EQ(parent.get_active_allocation_count(), 1);
        EXPECT_EQ(source.lifecycle(), nk::mem::AllocatorLifecycle::MovedFrom);
        EXPECT_EQ(destination.get_data(), source_backing);
        EXPECT_EQ(destination.get_active_allocation_count(), 1);

        auto* destination_address = &destination;
        destination.operator=(std::move(*destination_address));
        EXPECT_EQ(destination.get_data(), source_backing);
        EXPECT_TRUE(destination.reset(nk::mem::LinearResetMode::RetainContents));
    }

    EXPECT_EQ(parent.get_active_allocation_count(), 0);
}

TEST(LinearAllocator, RejectsMoveAssignmentIntoActiveDestination) {
    nk::mem::MallocAllocator parent{nk::mem::untracked};

    {
        nk::mem::LinearAllocator source{
            nk::mem::untracked,
            parent,
            128};
        nk::mem::LinearAllocator destination{
            nk::mem::untracked,
            parent,
            128};
        void* source_backing = source.get_data();
        void* destination_backing = destination.get_data();
        ASSERT_NE(source._allocate_raw(8, 8), nullptr);
        ASSERT_NE(destination._allocate_raw(8, 8), nullptr);

        destination = std::move(source);
        EXPECT_EQ(source.get_data(), source_backing);
        EXPECT_EQ(destination.get_data(), destination_backing);
        EXPECT_EQ(parent.get_active_allocation_count(), 2);

        EXPECT_TRUE(source.reset(nk::mem::LinearResetMode::RetainContents));
        EXPECT_TRUE(destination.reset(nk::mem::LinearResetMode::RetainContents));
    }

    EXPECT_EQ(parent.get_active_allocation_count(), 0);
}

TEST(LinearAllocator, PreservesTrackedInitializationAndResetCompatibility) {
    NK_MEMORY_SYSTEM_INIT();

    {
        nk::mem::MallocAllocator parent;
        ASSERT_NE(
            parent.allocator_init(
                nk::mem::MallocAllocator,
                "Linear backing",
                nk::MemoryType::Test),
            nullptr);

        nk::mem::LinearAllocator allocator;
        ASSERT_NE(
            allocator.allocator_init(
                nk::mem::LinearAllocator,
                "Linear arena",
                nk::MemoryType::Test,
                parent,
                KiB(1)),
            nullptr);

        ASSERT_NE(allocator.allocate_raw(500, alignof(nk::f32)), nullptr);
        EXPECT_GE(allocator.get_used_bytes(), 500);
        EXPECT_TRUE(allocator._free_linear_allocator());
        EXPECT_EQ(allocator.get_used_bytes(), 0);
        EXPECT_EQ(allocator.get_active_allocation_count(), 0);
    }

    NK_MEMORY_SYSTEM_SHUTDOWN();
}
