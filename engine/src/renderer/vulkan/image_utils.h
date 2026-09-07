#pragma once

#include "vulkan/vk.h"

namespace nk::vk {
    [[nodiscard]] constexpr u32 mip_level_count(const VkExtent2D extent) noexcept {
        if (extent.width == 0 || extent.height == 0)
            return 0;
        u32 largest = extent.width > extent.height ? extent.width : extent.height;
        u32 levels = 1;
        while (largest > 1) {
            largest >>= 1;
            ++levels;
        }
        return levels;
    }

    [[nodiscard]] constexpr VkExtent2D mip_extent(const VkExtent2D base, const u32 level) noexcept {
        if (base.width == 0 || base.height == 0)
            return {};
        const u32 width = level < 32 ? base.width >> level : 0;
        const u32 height = level < 32 ? base.height >> level : 0;
        return {width > 0 ? width : 1, height > 0 ? height : 1};
    }

    [[nodiscard]] constexpr bool supports_linear_mip_blit(const VkFormatFeatureFlags features) noexcept {
        constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_BLIT_SRC_BIT |
            VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        return (features & required) == required;
    }
}
