#include <gtest/gtest.h>

#include "collections/arr.h"
#include "collections/dyarr.h"
#include "memory/allocator_owner.h"
#include "memory/malloc_allocator.h"
#include "systems/memory_system.h"

namespace {
    class TrackedOwnedAllocator final : public nk::mem::MallocAllocator {
    public:
        static inline int destruction_count = 0;

        ~TrackedOwnedAllocator() override {
            ++destruction_count;
        }
    };
}

#if NK_MEMORY_TRACKING_ENABLED
TEST(AllocatorOwner, TracksOwnedContainerAllocatorsAndStorage) {
    TrackedOwnedAllocator::destruction_count = 0;
    auto& tracker = nk::mem::MemorySystem::init();
    ASSERT_EQ(tracker.state(), nk::mem::MemorySystemState::Ready);

    auto owner = nk::mem::AllocatorOwner::make_native<TrackedOwnedAllocator>(
        {__FILE__, __LINE__});
    ASSERT_TRUE(owner);
    auto* allocator = static_cast<TrackedOwnedAllocator*>(owner.get());
    ASSERT_NE(
        allocator->_allocator_init_tracked<TrackedOwnedAllocator>(
            tracker,
            __FILE__,
            __LINE__,
            "Owned arr allocator",
            nk::MemoryType::Test),
        nullptr);

    nk::cl::arr<nk::u64> array;
    ASSERT_TRUE(array.arr_init_own(std::move(owner), 4));
    EXPECT_EQ(allocator->get_active_allocation_count(), 1);
    EXPECT_TRUE(array.arr_shutdown());
    EXPECT_EQ(TrackedOwnedAllocator::destruction_count, 1);

    auto dynamic_owner =
        nk::mem::AllocatorOwner::make_native<TrackedOwnedAllocator>(
            {__FILE__, __LINE__});
    ASSERT_TRUE(dynamic_owner);
    auto* dynamic_allocator =
        static_cast<TrackedOwnedAllocator*>(dynamic_owner.get());
    ASSERT_NE(
        dynamic_allocator->_allocator_init_tracked<TrackedOwnedAllocator>(
            tracker,
            __FILE__,
            __LINE__,
            "Owned dyarr allocator",
            nk::MemoryType::Test),
        nullptr);

    nk::cl::dyarr<nk::u64> dynamic;
    ASSERT_TRUE(dynamic.dyarr_init_own(std::move(dynamic_owner), 4));
    EXPECT_EQ(dynamic_allocator->get_active_allocation_count(), 1);
    EXPECT_TRUE(dynamic.dyarr_shutdown());
    EXPECT_EQ(TrackedOwnedAllocator::destruction_count, 2);

    nk::mem::MemorySystem::shutdown();
}
#endif
