#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"
#include "renderer/render_view.h"

namespace {
    class RenderViewSystemTest : public testing::Test {
    protected:
        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        nk::RenderViewSystem views;
        nk::SceneLighting lighting{};
        glm::mat4 camera_view{1.0f};

        nk::RenderViewBuildData valid_input() {
            return {
                .world_view = &camera_view,
                .world_view_position = {1.0f, 2.0f, 3.0f},
                .lighting = &lighting,
            };
        }
    };
}

TEST_F(RenderViewSystemTest, BuildsWorldAndUiInConfiguredOrder) {
    ASSERT_TRUE(views.init(allocator, 1280, 720));
    nk::RenderViewPacket packets[
        nk::RenderViewSystem::maximum_render_view_count]{};
    auto built = views.build_packets(valid_input(), {packets});
    ASSERT_TRUE(built);
    ASSERT_EQ(*built, 2u);
    EXPECT_EQ(packets[0].type, nk::RenderViewType::world);
    EXPECT_EQ(packets[0].pass, nk::RenderPassKind::world);
    EXPECT_EQ(packets[0].view, camera_view);
    EXPECT_EQ(packets[0].view_position, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(packets[1].type, nk::RenderViewType::ui);
    EXPECT_EQ(packets[1].pass, nk::RenderPassKind::ui);
    EXPECT_EQ(packets[1].view, glm::mat4(1.0f));
}

TEST_F(RenderViewSystemTest, ValidatesBuildDataAndOutputCapacity) {
    ASSERT_TRUE(views.init(allocator, 1280, 720));
    nk::RenderViewPacket one_packet[1]{};
    auto too_small = views.build_packets(valid_input(), {one_packet});
    ASSERT_FALSE(too_small);
    EXPECT_EQ(too_small.error(), nk::render_view_error::output_too_small);

    auto invalid_input = valid_input();
    invalid_input.geometry_count = 1;
    nk::RenderViewPacket packets[
        nk::RenderViewSystem::maximum_render_view_count]{};
    auto invalid = views.build_packets(invalid_input, {packets});
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error(), nk::render_view_error::invalid_build_data);
}

TEST_F(RenderViewSystemTest, ResizesProjectionsWithoutPublishingZeroExtent) {
    ASSERT_TRUE(views.init(allocator, 1280, 720));
    nk::RenderViewPacket before[
        nk::RenderViewSystem::maximum_render_view_count]{};
    ASSERT_TRUE(views.build_packets(valid_input(), {before}));

    auto zero = views.resize(0, 720);
    ASSERT_FALSE(zero);
    EXPECT_EQ(zero.error(), nk::render_view_error::invalid_extent);
    EXPECT_EQ(views.width(), 1280u);
    EXPECT_EQ(views.height(), 720u);

    ASSERT_TRUE(views.resize(800, 800));
    nk::RenderViewPacket after[
        nk::RenderViewSystem::maximum_render_view_count]{};
    ASSERT_TRUE(views.build_packets(valid_input(), {after}));
    EXPECT_NE(before[0].projection, after[0].projection);
    EXPECT_NE(before[1].projection, after[1].projection);
}

TEST_F(RenderViewSystemTest, RejectsDuplicateOrderAndInvalidatesRemovedHandles) {
    ASSERT_TRUE(views.init(allocator, 1280, 720, 3));
    auto duplicate_order = views.create({
        .name = "duplicate",
        .type = nk::RenderViewType::world,
        .pass = nk::RenderPassKind::world,
        .projection = nk::ProjectionType::perspective,
        .order = 1,
    });
    ASSERT_FALSE(duplicate_order);
    EXPECT_EQ(duplicate_order.error(), nk::render_view_error::duplicate_order);

    auto extra = views.create({
        .name = "extra",
        .type = nk::RenderViewType::world,
        .pass = nk::RenderPassKind::world,
        .projection = nk::ProjectionType::perspective,
        .order = 3,
    });
    ASSERT_TRUE(extra);
    EXPECT_TRUE(views.valid(*extra));
    ASSERT_TRUE(views.remove(*extra));
    EXPECT_FALSE(views.valid(*extra));

    auto reused = views.create({
        .name = "replacement",
        .type = nk::RenderViewType::world,
        .pass = nk::RenderPassKind::world,
        .projection = nk::ProjectionType::perspective,
        .order = 3,
    });
    ASSERT_TRUE(reused);
    EXPECT_EQ(reused->index, extra->index);
    EXPECT_NE(reused->generation, extra->generation);
}
