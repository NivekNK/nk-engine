#include <cstring>

#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"
#include "platform/file.h"
#include "resources/image_loader.h"

namespace {
    constexpr nk::cstr cobblestone_path =
        NK_TEST_ASSET_ROOT "/textures/cobblestone.png";
    constexpr nk::cstr paving_path =
        NK_TEST_ASSET_ROOT "/textures/paving.png";
    constexpr nk::cstr paving2_path =
        NK_TEST_ASSET_ROOT "/textures/paving2.png";
}

TEST(ImageLoader, DecodesChapterTexturesAsRgba) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    {
        auto cobblestone = nk::ImageLoader::load_png(
            allocator, cobblestone_path);
        ASSERT_TRUE(cobblestone);
        EXPECT_EQ(cobblestone->width, 512u);
        EXPECT_EQ(cobblestone->height, 512u);
        EXPECT_EQ(cobblestone->channel_count, 4u);
        EXPECT_EQ(cobblestone->pixels.length(), 512u * 512u * 4u);
        EXPECT_FALSE(cobblestone->has_transparency);

        auto paving = nk::ImageLoader::load_png(allocator, paving_path);
        ASSERT_TRUE(paving);
        EXPECT_EQ(paving->width, 480u);
        EXPECT_EQ(paving->height, 480u);
        EXPECT_FALSE(paving->has_transparency);

        auto paving2 = nk::ImageLoader::load_png(allocator, paving2_path);
        ASSERT_TRUE(paving2);
        EXPECT_EQ(paving2->width, 480u);
        EXPECT_EQ(paving2->height, 480u);
        // The source is encoded as RGBA, but all alpha samples are opaque.
        EXPECT_FALSE(paving2->has_transparency);

        EXPECT_EQ(allocator.get_active_allocation_count(), 3u);
    }

    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(ImageLoader, FlipsRowsWithoutASecondPixelAllocation) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    auto original = nk::ImageLoader::load_png(
        allocator, cobblestone_path, false);
    ASSERT_TRUE(original);
    auto flipped = nk::ImageLoader::load_png(
        allocator, cobblestone_path, true);
    ASSERT_TRUE(flipped);

    const nk::u64 row_size = original->width * original->channel_count;
    EXPECT_EQ(
        std::memcmp(
            original->pixels.data(),
            flipped->pixels.data() + (flipped->height - 1) * row_size,
            row_size),
        0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 2u);
}

TEST(ImageLoader, PreservesFileAndDecoderFailures) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    auto missing = nk::ImageLoader::load_png(
        allocator, NK_TEST_ASSET_ROOT "/textures/missing.png");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, nk::image_error_code::file_failed);
    EXPECT_EQ(missing.error().file, nk::file_error::not_found);

    auto invalid = nk::ImageLoader::load_png(
        allocator, NK_TEST_ASSET_ROOT "/models/cube.obj");
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, nk::image_error_code::decode_failed);
    EXPECT_NE(invalid.error().native_code, 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}
