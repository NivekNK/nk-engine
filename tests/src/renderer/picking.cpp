#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"
#include "renderer/picking.h"

TEST(PickRegistry, AcquiresResolvesAndInvalidatesGenerationalIds) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::PickRegistry registry;
    ASSERT_TRUE(registry.init(allocator, 2));

    auto first = registry.acquire({nk::PickObjectKind::geometry, 41});
    ASSERT_TRUE(first);
    EXPECT_NE(first->value, 0u);
    ASSERT_TRUE(registry.resolve(*first));
    EXPECT_EQ(
        *registry.resolve(*first),
        (nk::PickObject{nk::PickObjectKind::geometry, 41}));

    ASSERT_TRUE(registry.release(*first));
    auto stale = registry.resolve(*first);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error(), nk::pick_error::stale_id);

    auto reused = registry.acquire({nk::PickObjectKind::ui, 82});
    ASSERT_TRUE(reused);
    EXPECT_NE(reused->value, first->value);
    EXPECT_EQ(registry.count(), 1u);
}

TEST(PickRegistry, EnforcesCapacityAndRejectsInvalidOperations) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::PickRegistry registry;
    EXPECT_EQ(
        registry.acquire({nk::PickObjectKind::custom, 1}).error(),
        nk::pick_error::not_initialized);
    EXPECT_EQ(
        registry.init(allocator, 0).error(),
        nk::pick_error::invalid_capacity);
    ASSERT_TRUE(registry.init(allocator, 1));
    EXPECT_EQ(
        registry.acquire({}).error(),
        nk::pick_error::invalid_object);
    auto id = registry.acquire({nk::PickObjectKind::custom, 1});
    ASSERT_TRUE(id);
    EXPECT_EQ(
        registry.acquire({nk::PickObjectKind::custom, 2}).error(),
        nk::pick_error::capacity_exceeded);
    EXPECT_EQ(
        registry.release({}).error(),
        nk::pick_error::invalid_id);
    ASSERT_TRUE(registry.release(*id));
    EXPECT_EQ(
        registry.release(*id).error(),
        nk::pick_error::stale_id);
}
