#include <gtest/gtest.h>
#include "renderer/text_draw.h"
#include "memory/malloc_allocator.h"

namespace {
    using namespace nk;
    class TextDrawTest : public ::testing::Test {
    protected:
        mem::MallocAllocator allocator{mem::untracked};
        FontSystem fonts;
        TextDrawList list;
        TextLayout layout;
        FontHandle font;
        void SetUp() override {
            ASSERT_TRUE(fonts.init(allocator));
            auto loaded = fonts.load(NK_TEST_ASSET_ROOT "/fonts/NotoSans-Regular.ttf");
            ASSERT_TRUE(loaded); font = *loaded;
            ASSERT_TRUE(fonts.layout(font, "ABC ", {}, layout));
            ASSERT_TRUE(list.init(allocator, fonts));
            ASSERT_TRUE(list.begin(800, 600));
        }
    };
}

TEST_F(TextDrawTest, BatchesAdjacentTextAndColorChangesInSubmissionOrder) {
    ASSERT_TRUE(list.append(layout, {10, 10}, {1, 0, 0, 1}));
    ASSERT_TRUE(list.append(layout, {10, 40}, {0, 1, 0, .5f}));
    ASSERT_EQ(list.batches().length(), 1);
    EXPECT_EQ(list.batches()[0].vertex_count, 36);
    ASSERT_EQ(list.vertices().length(), 36);
    EXPECT_EQ(list.vertices()[0].color, glm::vec4(1, 0, 0, 1));
    EXPECT_EQ(list.vertices()[18].color, glm::vec4(0, 1, 0, .5f));
    EXPECT_LT(list.vertices()[0].position.y, list.vertices()[18].position.y);
}

TEST_F(TextDrawTest, PreservesPickIdentityAsABatchBoundary) {
    const PickId first{0x00010001u};
    const PickId second{0x00010002u};
    ASSERT_TRUE(list.append(
        layout,
        {10, 10},
        {1, 1, 1, 1},
        {0, 0, 800, 600},
        first));
    ASSERT_TRUE(list.append(
        layout,
        {10, 40},
        {1, 1, 1, 1},
        {0, 0, 800, 600},
        second));
    ASSERT_EQ(list.batches().length(), 2u);
    EXPECT_EQ(list.batches()[0].pick_id, first);
    EXPECT_EQ(list.batches()[1].pick_id, second);
}

TEST_F(TextDrawTest, ClipsToTheFramebufferAndPreservesBatchBoundaries) {
    ASSERT_TRUE(list.append(layout, {10, 10}, {1, 1, 1, 1}, {-20, -10, 100, 70}));
    ASSERT_TRUE(list.append(layout, {10, 10}, {1, 1, 1, 1}, {10, 5, 900, 800}));
    ASSERT_TRUE(list.append(layout, {10, 10}, {1, 1, 1, 1}, {-20, -10, 100, 70}));
    ASSERT_EQ(list.batches().length(), 3);
    EXPECT_EQ(list.batches()[0].scissor.x, 0);
    EXPECT_EQ(list.batches()[0].scissor.width, 80);
    EXPECT_EQ(list.batches()[1].scissor.x, 10);
    EXPECT_EQ(list.batches()[1].scissor.width, 790);
    EXPECT_EQ(list.batches()[1].scissor.height, 595);
    EXPECT_EQ(list.batches()[2].first_vertex, 36);
}

TEST_F(TextDrawTest, DoesNotDrawWhitespaceTransparentOrFullyClippedRuns) {
    ASSERT_TRUE(list.append(layout, {900, 10}));
    ASSERT_TRUE(list.append(layout, {10, 10}, {1, 1, 1, 0}));
    ASSERT_TRUE(list.append(layout, {10, 10}, {1, 1, 1, 1}, {}));
    ASSERT_TRUE(fonts.layout(font, " \t\n", {}, layout));
    ASSERT_TRUE(list.append(layout, {10, 10}));
    EXPECT_TRUE(list.vertices().empty());
    EXPECT_TRUE(list.batches().empty());
}

TEST_F(TextDrawTest, ConvertsLogicalCoordinatesAndClippingAtFractionalScale) {
    ASSERT_TRUE(fonts.layout(font, "A", {.size=20, .scale=1.5f}, layout));
    ASSERT_TRUE(list.begin(800, 600, 1.5f));
    ASSERT_TRUE(list.append(layout, {10, 10}, {1, 1, 1, 1}, {1, 1, 100, 100}));
    auto raster = fonts.rasterize(layout, layout.glyphs[0].id); ASSERT_TRUE(raster);
    EXPECT_FLOAT_EQ(list.vertices()[0].position.x, 15 + raster->bearing_x);
    EXPECT_EQ(list.batches()[0].scissor.x, 1);
    EXPECT_EQ(list.batches()[0].scissor.width, 151);
    ASSERT_TRUE(list.begin(800, 600));
    EXPECT_FALSE(list.append(layout, {10, 10})); // Caller must reshape for DPI.
}

TEST_F(TextDrawTest, ReusesGeometryAndAtlasStorageAfterWarmup) {
    ASSERT_TRUE(list.append(layout, {10, 10}));
    const auto* vertices = list.vertices().data();
    const auto* batches = list.batches().data();
    const u64 allocations = allocator.get_active_allocation_count();
    const auto stats = fonts.statistics();
    for (u32 i = 0; i < 100; ++i) {
        ASSERT_TRUE(list.begin(800, 600));
        ASSERT_TRUE(list.append(layout, {10, 10}));
    }
    EXPECT_EQ(list.vertices().data(), vertices);
    EXPECT_EQ(list.batches().data(), batches);
    EXPECT_EQ(allocator.get_active_allocation_count(), allocations);
    EXPECT_EQ(fonts.statistics().rasterized_glyphs, stats.rasterized_glyphs);
    EXPECT_EQ(fonts.statistics().shape_calls, stats.shape_calls);
}

TEST(TextDraw, RollsBackAnAppendWhenTheAtlasIsFull) {
    mem::MallocAllocator allocator{mem::untracked};
    FontSystem fonts;
    ASSERT_TRUE(fonts.init(allocator, {.atlas_size=32, .max_atlas_pages=1}));
    auto font = fonts.load(NK_TEST_ASSET_ROOT "/fonts/NotoSans-Regular.ttf"); ASSERT_TRUE(font);
    TextDrawList list;
    ASSERT_TRUE(list.init(allocator, fonts));
    ASSERT_TRUE(list.begin(800, 600));
    TextLayout first, excess;
    ASSERT_TRUE(fonts.layout(*font, "A", {}, first));
    ASSERT_TRUE(fonts.layout(*font, "ABCDEFGHIJKLMNOP", {}, excess));
    ASSERT_TRUE(list.append(first, {10, 10}));
    EXPECT_FALSE(list.append(excess, {10, 40}));
    ASSERT_EQ(list.vertices().length(), 6);
    ASSERT_EQ(list.batches().length(), 1);
    EXPECT_EQ(list.batches()[0].vertex_count, 6);
}
