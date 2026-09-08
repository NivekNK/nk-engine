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

TEST(Picking, ConvertsLogicalCoordinatesToTopLeftPhysicalTexels) {
    auto unity = nk::physical_pick_position(12.9f, 7.1f, 1.0f, 100, 50);
    ASSERT_TRUE(unity);
    EXPECT_EQ(*unity, (nk::PickPosition{12, 7}));

    auto fractional = nk::physical_pick_position(
        11.0f, 9.0f, 1.25f, 100, 50);
    ASSERT_TRUE(fractional);
    EXPECT_EQ(*fractional, (nk::PickPosition{13, 11}));

    auto clamped = nk::physical_pick_position(
        -4.0f, 200.0f, 1.5f, 100, 50);
    ASSERT_TRUE(clamped);
    EXPECT_EQ(*clamped, (nk::PickPosition{0, 49}));

    EXPECT_EQ(
        nk::physical_pick_position(1, 1, 0, 100, 50).error(),
        nk::pick_error::invalid_position);
    EXPECT_EQ(
        nk::physical_pick_position(1, 1, 1, 0, 50).error(),
        nk::pick_error::invalid_position);
}

TEST(Picking, CoalescesHoverAndPreservesClickOrder) {
    nk::PickQueue queue;
    ASSERT_TRUE(queue.submit({1, 1}, 3, nk::PickRequestKind::hover));
    auto first_click = queue.submit({2, 2}, 3, nk::PickRequestKind::click);
    ASSERT_TRUE(first_click);
    ASSERT_TRUE(queue.submit({3, 3}, 3, nk::PickRequestKind::hover));
    auto second_click = queue.submit({4, 4}, 3, nk::PickRequestKind::click);
    ASSERT_TRUE(second_click);

    nk::PickRequest request;
    ASSERT_TRUE(queue.consume(request));
    EXPECT_EQ(request.sequence, *first_click);
    ASSERT_TRUE(queue.consume(request));
    EXPECT_EQ(request.position, (nk::PickPosition{3, 3}));
    ASSERT_TRUE(queue.consume(request));
    EXPECT_EQ(request.sequence, *second_click);
    EXPECT_FALSE(queue.consume(request));
}

TEST(Picking, PublishesNewestRetiredResultAndDiscardsStaleScene) {
    nk::PickQueue queue;
    auto sequence = queue.submit({9, 8}, 17, nk::PickRequestKind::hover);
    ASSERT_TRUE(sequence);
    nk::PickRequest request;
    ASSERT_TRUE(queue.consume(request));
    queue.publish({
        .request = request,
        .id = {77},
        .object = {nk::PickObjectKind::geometry, 4},
        .retired_frame = 12,
    });

    queue.discard_scene(18);
    nk::PickResult result;
    EXPECT_FALSE(queue.poll(result));

    auto current = queue.submit({4, 5}, 18, nk::PickRequestKind::click);
    ASSERT_TRUE(current);
    ASSERT_TRUE(queue.consume(request));
    queue.publish({.request = request, .retired_frame = 14});
    ASSERT_TRUE(queue.poll(result));
    EXPECT_EQ(result.request.sequence, *current);
    EXPECT_FALSE(result.hit());
}
