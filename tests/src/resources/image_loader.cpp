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
    constexpr nk::cstr orange_lines_path =
        NK_TEST_ASSET_ROOT "/textures/orange_lines_512.png";
    constexpr nk::cstr cobblestone_specular_path =
        NK_TEST_ASSET_ROOT "/textures/cobblestone_SPEC.png";
    constexpr nk::cstr paving_specular_path =
        NK_TEST_ASSET_ROOT "/textures/paving_SPEC.png";
    constexpr nk::cstr paving2_specular_path =
        NK_TEST_ASSET_ROOT "/textures/paving2_SPEC.png";
    constexpr nk::cstr orange_lines_specular_path =
        NK_TEST_ASSET_ROOT "/textures/orange_lines_512_SPEC.png";
    constexpr nk::cstr cobblestone_normal_path =
        NK_TEST_ASSET_ROOT "/textures/cobblestone_NRM.png";
    constexpr nk::cstr paving_normal_path =
        NK_TEST_ASSET_ROOT "/textures/paving_NRM.png";
    constexpr nk::cstr paving2_normal_path =
        NK_TEST_ASSET_ROOT "/textures/paving2_NRM.png";
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

        auto orange_lines = nk::ImageLoader::load_png(
            allocator, orange_lines_path);
        ASSERT_TRUE(orange_lines);
        EXPECT_EQ(orange_lines->width, 512u);
        EXPECT_EQ(orange_lines->height, 512u);
        EXPECT_EQ(orange_lines->channel_count, 4u);

        auto cobblestone_specular = nk::ImageLoader::load_png(
            allocator, cobblestone_specular_path);
        ASSERT_TRUE(cobblestone_specular);
        EXPECT_EQ(cobblestone_specular->width, 512u);
        EXPECT_EQ(cobblestone_specular->height, 512u);
        EXPECT_FALSE(cobblestone_specular->has_transparency);

        auto paving_specular = nk::ImageLoader::load_png(
            allocator, paving_specular_path);
        ASSERT_TRUE(paving_specular);
        EXPECT_EQ(paving_specular->width, 480u);
        EXPECT_EQ(paving_specular->height, 480u);

        auto paving2_specular = nk::ImageLoader::load_png(
            allocator, paving2_specular_path);
        ASSERT_TRUE(paving2_specular);
        EXPECT_EQ(paving2_specular->width, 480u);
        EXPECT_EQ(paving2_specular->height, 480u);

        auto orange_lines_specular = nk::ImageLoader::load_png(
            allocator, orange_lines_specular_path);
        ASSERT_TRUE(orange_lines_specular);
        EXPECT_EQ(orange_lines_specular->width, 512u);
        EXPECT_EQ(orange_lines_specular->height, 512u);

        auto cobblestone_normal = nk::ImageLoader::load_png(
            allocator, cobblestone_normal_path);
        ASSERT_TRUE(cobblestone_normal);
        EXPECT_EQ(cobblestone_normal->width, 512u);
        EXPECT_EQ(cobblestone_normal->height, 512u);
        EXPECT_FALSE(cobblestone_normal->has_transparency);

        auto paving_normal = nk::ImageLoader::load_png(
            allocator, paving_normal_path);
        ASSERT_TRUE(paving_normal);
        EXPECT_EQ(paving_normal->width, 480u);
        EXPECT_EQ(paving_normal->height, 480u);

        auto paving2_normal = nk::ImageLoader::load_png(
            allocator, paving2_normal_path);
        ASSERT_TRUE(paving2_normal);
        EXPECT_EQ(paving2_normal->width, 480u);
        EXPECT_EQ(paving2_normal->height, 480u);

        EXPECT_EQ(allocator.get_active_allocation_count(), 11u);
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
