#pragma once

#include "resources/texture.h"
#include "vulkan/vk.h"

namespace nk::vk {
    [[nodiscard]] constexpr VkFormat texture_format(
        const TextureFormat format) noexcept {
        switch (format) {
            case TextureFormat::r8_unorm: return VK_FORMAT_R8_UNORM;
            case TextureFormat::rg8_unorm: return VK_FORMAT_R8G8_UNORM;
            case TextureFormat::rgb8_unorm: return VK_FORMAT_R8G8B8_UNORM;
            case TextureFormat::rgba8_unorm: return VK_FORMAT_R8G8B8A8_UNORM;
            case TextureFormat::rgba8_srgb: return VK_FORMAT_R8G8B8A8_SRGB;
            case TextureFormat::bgra8_unorm: return VK_FORMAT_B8G8R8A8_UNORM;
            case TextureFormat::bgra8_srgb: return VK_FORMAT_B8G8R8A8_SRGB;
            case TextureFormat::r32_uint: return VK_FORMAT_R32_UINT;
            case TextureFormat::depth32_float: return VK_FORMAT_D32_SFLOAT;
            case TextureFormat::depth32_float_stencil8:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case TextureFormat::depth24_unorm_stencil8:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case TextureFormat::unknown: return VK_FORMAT_UNDEFINED;
        }
        return VK_FORMAT_UNDEFINED;
    }

    [[nodiscard]] constexpr TextureFormat texture_format(
        const VkFormat format) noexcept {
        switch (format) {
            case VK_FORMAT_R8_UNORM: return TextureFormat::r8_unorm;
            case VK_FORMAT_R8G8_UNORM: return TextureFormat::rg8_unorm;
            case VK_FORMAT_R8G8B8_UNORM: return TextureFormat::rgb8_unorm;
            case VK_FORMAT_R8G8B8A8_UNORM: return TextureFormat::rgba8_unorm;
            case VK_FORMAT_R8G8B8A8_SRGB: return TextureFormat::rgba8_srgb;
            case VK_FORMAT_B8G8R8A8_UNORM: return TextureFormat::bgra8_unorm;
            case VK_FORMAT_B8G8R8A8_SRGB: return TextureFormat::bgra8_srgb;
            case VK_FORMAT_R32_UINT: return TextureFormat::r32_uint;
            case VK_FORMAT_D32_SFLOAT: return TextureFormat::depth32_float;
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return TextureFormat::depth32_float_stencil8;
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return TextureFormat::depth24_unorm_stencil8;
            default: return TextureFormat::unknown;
        }
    }

    [[nodiscard]] constexpr VkSampleCountFlagBits sample_count(
        const TextureSampleCount count) noexcept {
        return static_cast<VkSampleCountFlagBits>(count);
    }
}
