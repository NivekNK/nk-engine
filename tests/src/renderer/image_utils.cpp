#include <gtest/gtest.h>

#include "vulkan/image_utils.h"

TEST(ImageMips, CountsCompleteChainsWithoutFloatingPoint) {
    EXPECT_EQ(nk::vk::mip_level_count({0, 1}), 0);
    EXPECT_EQ(nk::vk::mip_level_count({1, 0}), 0);
    EXPECT_EQ(nk::vk::mip_level_count({1, 1}), 1);
    EXPECT_EQ(nk::vk::mip_level_count({1024, 1024}), 11);
    EXPECT_EQ(nk::vk::mip_level_count({1, 512}), 10);
    EXPECT_EQ(nk::vk::mip_level_count({1000, 23}), 10);
    EXPECT_EQ(nk::vk::mip_level_count({0xffffffffu, 1}), 32);
}

TEST(ImageMips, ExtentsClampEachDimensionWithoutZeroOrOvershift) {
    constexpr VkExtent2D base{1000, 23};
    const auto original = nk::vk::mip_extent(base, 0);
    EXPECT_EQ(original.width, base.width);
    EXPECT_EQ(original.height, base.height);
    const auto half = nk::vk::mip_extent(base, 1);
    EXPECT_EQ(half.width, 500);
    EXPECT_EQ(half.height, 11);
    const auto thin = nk::vk::mip_extent(base, 5);
    EXPECT_EQ(thin.width, 31);
    EXPECT_EQ(thin.height, 1);
    const auto last = nk::vk::mip_extent(base, 9);
    EXPECT_EQ(last.width, 1);
    EXPECT_EQ(last.height, 1);
    EXPECT_EQ(nk::vk::mip_extent(base, 32).width, 1);
    EXPECT_EQ(nk::vk::mip_extent({0, 2}, 0).height, 0);
}

TEST(ImageMips, RequiresBothBlitDirectionsAndLinearFiltering) {
    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_BLIT_SRC_BIT |
        VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    EXPECT_TRUE(nk::vk::supports_linear_mip_blit(required));
    EXPECT_TRUE(nk::vk::supports_linear_mip_blit(required | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT));
    EXPECT_FALSE(nk::vk::supports_linear_mip_blit(0));
    EXPECT_FALSE(nk::vk::supports_linear_mip_blit(required & ~VK_FORMAT_FEATURE_BLIT_SRC_BIT));
    EXPECT_FALSE(nk::vk::supports_linear_mip_blit(required & ~VK_FORMAT_FEATURE_BLIT_DST_BIT));
    EXPECT_FALSE(nk::vk::supports_linear_mip_blit(required & ~VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT));
}
