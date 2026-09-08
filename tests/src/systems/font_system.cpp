#include <gtest/gtest.h>
#include <cmath>
#include "systems/font_system.h"
#include "memory/malloc_allocator.h"

namespace {
    using namespace nk;
    const strview font_path{NK_TEST_ASSET_ROOT "/fonts/NotoSans-Regular.ttf"};
    class FontTest : public ::testing::Test {
    protected:
        mem::MallocAllocator allocator{mem::untracked};
        FontSystem fonts;
        FontHandle font;
        void SetUp() override {
            ASSERT_TRUE(fonts.init(allocator));
            auto loaded = fonts.load(font_path);
            ASSERT_TRUE(loaded);
            font = *loaded;
        }
    };
    class BudgetAllocator final : public mem::MallocAllocator {
    public:
        BudgetAllocator() : MallocAllocator{mem::untracked} {}
        u64 budget = numeric::u64_max;
    protected:
        void* _do_allocate(u64 bytes, u64 alignment) noexcept override {
            if (budget == 0) return nullptr;
            --budget;
            return MallocAllocator::_do_allocate(bytes, alignment);
        }
    };
}

TEST_F(FontTest, MeasuresWithTheSameShapingWithoutAllocatingAnAtlas) {
    TextLayout layout;
    ASSERT_TRUE(fonts.layout(font, "¡Hola! áéíóú ñ", {}, layout));
    auto measured = fonts.measure(font, "¡Hola! áéíóú ñ");
    ASSERT_TRUE(measured);
    EXPECT_FLOAT_EQ(measured->width, layout.metrics.width);
    EXPECT_FLOAT_EQ(measured->height, layout.metrics.height);
    EXPECT_GT(measured->width, 40);
    EXPECT_GT(measured->line_height, 0);
    EXPECT_EQ(fonts.page_count(), 0);
    EXPECT_EQ(fonts.statistics().rasterized_glyphs, 0);
    EXPECT_EQ(layout.glyphs[0].cluster, 0);
    EXPECT_EQ(layout.glyphs[1].cluster, 2); // Byte, not codepoint index.
    for (const auto& glyph : layout.glyphs) EXPECT_NE(glyph.id, 0);
}

TEST_F(FontTest, ShapesLigaturesCombiningMarksAndExplicitDirections) {
    TextLayout ligatures, separate, combined, composed, rtl;
    ASSERT_TRUE(fonts.layout(font, "office", {}, ligatures));
    TextOptions options; options.ligatures = false;
    ASSERT_TRUE(fonts.layout(font, "office", options, separate));
    EXPECT_LT(ligatures.glyphs.length(), separate.glyphs.length());
    ASSERT_TRUE(fonts.layout(font, "a\xcc\x81", {}, combined));
    ASSERT_TRUE(fonts.layout(font, "á", {}, composed));
    EXPECT_EQ(combined.glyphs.length(), composed.glyphs.length());
    EXPECT_FLOAT_EQ(combined.metrics.width, composed.metrics.width);
    options.direction = TextDirection::right_to_left;
    ASSERT_TRUE(fonts.layout(font, "ABC", options, rtl));
    ASSERT_EQ(rtl.glyphs.length(), 3);
    EXPECT_EQ(rtl.glyphs[0].cluster, 2);
    EXPECT_EQ(rtl.glyphs[2].cluster, 0);
}

TEST_F(FontTest, HandlesLinesTabsEmptyTextAndPhysicalScale) {
    TextLayout lines, empty, scaled, normal;
    ASSERT_TRUE(fonts.layout(font, "A\tB\r\nC\n", {}, lines));
    EXPECT_EQ(lines.metrics.line_count, 3);
    EXPECT_FLOAT_EQ(lines.metrics.height, 3 * lines.metrics.line_height);
    ASSERT_EQ(lines.glyphs.length(), 3);
    EXPECT_GT(lines.glyphs[1].x, lines.glyphs[0].advance);
    EXPECT_FLOAT_EQ(lines.glyphs[2].baseline,
        lines.metrics.ascender + lines.metrics.line_height);
    ASSERT_TRUE(fonts.layout(font, {}, {}, empty));
    EXPECT_EQ(empty.metrics.width, 0);
    EXPECT_EQ(empty.metrics.height, 0);
    EXPECT_TRUE(empty.glyphs.empty());
    ASSERT_TRUE(fonts.layout(font, "Hello", {}, normal));
    TextOptions options; options.scale = 2;
    ASSERT_TRUE(fonts.layout(font, "Hello", options, scaled));
    EXPECT_EQ(scaled.pixel_size_64, 2 * normal.pixel_size_64);
    EXPECT_NEAR(scaled.metrics.width, normal.metrics.width, 3);
}

TEST_F(FontTest, RejectsBadInputAndPreservesThePreviousLayout) {
    TextLayout layout;
    ASSERT_TRUE(fonts.layout(font, "previous", {}, layout));
    const auto* storage = layout.glyphs.data();
    const f32 width = layout.metrics.width;
    auto invalid = fonts.layout(font, "\xed\xa0\x80", {}, layout);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error(), text_error::invalid_utf8);
    TextOptions options; options.size = NAN;
    EXPECT_FALSE(fonts.layout(font, "text", options, layout));
    EXPECT_FALSE(fonts.layout({}, "text", {}, layout));
    EXPECT_EQ(storage, layout.glyphs.data());
    EXPECT_EQ(width, layout.metrics.width);
    mem::MallocAllocator another{mem::untracked};
    TextLayout borrowed;
    ASSERT_TRUE(borrowed.glyphs.dyarr_init(&another, 1));
    EXPECT_FALSE(fonts.layout(font, "text", {}, borrowed));
}

TEST_F(FontTest, KeepsAtlasCoordinatesStableAndUploadsOnlyDirtyPages) {
    TextLayout layout;
    ASSERT_TRUE(fonts.layout(font, "AB ", {}, layout));
    auto a = fonts.rasterize(layout, layout.glyphs[0].id);
    ASSERT_TRUE(a);
    ASSERT_NE(a->page, numeric::invalid_id);
    auto first = fonts.page(a->page);
    EXPECT_EQ(first.pixels.length(), 1024u * 1024u);
    EXPECT_GT(first.dirty.width, 0);
    EXPECT_EQ(first.pixels[(a->y - 1) * first.width + a->x], 0);
    bool coverage = false;
    for (u32 y = 0; y < a->height; ++y)
        for (u32 x = 0; x < a->width; ++x)
            coverage |= first.pixels[(a->y + y) * first.width + a->x + x] != 0;
    EXPECT_TRUE(coverage);
    auto b = fonts.rasterize(layout, layout.glyphs[1].id);
    ASSERT_TRUE(b);
    fonts.acknowledge_upload(a->page, first.revision); // Stale receipt.
    EXPECT_GT(fonts.page(a->page).dirty.width, 0);
    const auto second = fonts.page(a->page);
    fonts.acknowledge_upload(a->page, second.revision);
    EXPECT_EQ(fonts.page(a->page).dirty.width, 0);
    auto cached = fonts.rasterize(layout, layout.glyphs[0].id);
    ASSERT_TRUE(cached);
    EXPECT_EQ(cached->x, a->x); EXPECT_EQ(cached->y, a->y);
    EXPECT_EQ(fonts.page(a->page).revision, second.revision);
    auto space = fonts.rasterize(layout, layout.glyphs[2].id);
    ASSERT_TRUE(space);
    EXPECT_EQ(space->page, numeric::invalid_id);
}

TEST_F(FontTest, InvalidatesHandlesAcrossReloadAndShutdown) {
    TextLayout layout;
    ASSERT_TRUE(fonts.layout(font, "A", {}, layout));
    ASSERT_TRUE(fonts.unload(font));
    EXPECT_FALSE(fonts.valid(font));
    auto replacement = fonts.load(font_path);
    ASSERT_TRUE(replacement);
    EXPECT_NE(replacement->generation, font.generation);
    EXPECT_FALSE(fonts.rasterize(layout, layout.glyphs[0].id));
    FontSystem other;
    ASSERT_TRUE(other.init(allocator));
    EXPECT_FALSE(other.valid(*replacement));
    fonts.shutdown();
    ASSERT_TRUE(fonts.init(allocator));
    EXPECT_FALSE(fonts.valid(*replacement));
}

TEST(Fonts, BoundsAtlasCapacityWithoutRelocatingExistingGlyphs) {
    mem::MallocAllocator allocator{mem::untracked};
    FontSystem fonts;
    ASSERT_TRUE(fonts.init(allocator, {.atlas_size=32, .max_atlas_pages=1}));
    auto font = fonts.load(font_path); ASSERT_TRUE(font);
    TextLayout layout;
    ASSERT_TRUE(fonts.layout(*font, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", {}, layout));
    auto first = fonts.rasterize(layout, layout.glyphs[0].id); ASSERT_TRUE(first);
    bool exhausted = false;
    for (auto glyph : layout.glyphs) {
        auto raster = fonts.rasterize(layout, glyph.id);
        if (!raster) { EXPECT_EQ(raster.error(), text_error::atlas_full); exhausted = true; break; }
    }
    EXPECT_TRUE(exhausted);
    auto stable = fonts.rasterize(layout, layout.glyphs[0].id); ASSERT_TRUE(stable);
    EXPECT_EQ(stable->x, first->x); EXPECT_EQ(stable->y, first->y);
}

TEST(Fonts, ReleasesOwnedMemoryAndRollsBackInitializationFailures) {
    for (u64 budget : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 128u, 512u}) {
        BudgetAllocator allocator;
        allocator.budget = budget;
        {
            FontSystem fonts;
            auto initialized = fonts.init(allocator);
            if (initialized) {
                auto loaded = fonts.load(font_path);
                if (loaded) {
                    TextLayout layout;
                    auto shaped = fonts.layout(*loaded, "memory", {}, layout);
                    if (shaped)
                        for (const auto& glyph : layout.glyphs)
                            (void)fonts.rasterize(layout, glyph.id);
                }
            }
        }
        EXPECT_EQ(allocator.get_active_allocation_count(), 0) << budget;
    }
}

TEST_F(FontTest, RejectsMissingAndNonFontAssets) {
    EXPECT_FALSE(fonts.load("not-a-font.ttf"));
    EXPECT_FALSE(fonts.load(NK_TEST_ASSET_ROOT "/fonts/OFL.txt"));
    EXPECT_FALSE(fonts.load(font_path, 5));
    EXPECT_EQ(fonts.statistics().fonts, 1);
}
