#pragma once

#include "core/defines.h"

namespace nk {
    enum class TextureDimension : u8 {
        texture_2d,
        cube,
    };

    enum class TextureState : u8 {
        unloaded,
        queued,
        loading,
        ready,
        failed,
        cancelling,
    };

    enum class TextureFormat : u8 {
        unknown,
        r8_unorm,
        rg8_unorm,
        rgb8_unorm,
        rgba8_unorm,
        rgba8_srgb,
        bgra8_unorm,
        bgra8_srgb,
        r32_uint,
        depth32_float,
        depth32_float_stencil8,
        depth24_unorm_stencil8,
    };

    enum class TextureSampleCount : u8 {
        one = 1,
        two = 2,
        four = 4,
        eight = 8,
        sixteen = 16,
        thirty_two = 32,
        sixty_four = 64,
    };

    [[nodiscard]] constexpr bool is_color_format(
        const TextureFormat format) noexcept {
        return format >= TextureFormat::r8_unorm &&
               format <= TextureFormat::r32_uint;
    }

    [[nodiscard]] constexpr bool is_depth_format(
        const TextureFormat format) noexcept {
        return format >= TextureFormat::depth32_float &&
               format <= TextureFormat::depth24_unorm_stencil8;
    }

    [[nodiscard]] constexpr TextureFormat unorm_texture_format(
        const u8 channel_count) noexcept {
        switch (channel_count) {
            case 1: return TextureFormat::r8_unorm;
            case 2: return TextureFormat::rg8_unorm;
            case 3: return TextureFormat::rgb8_unorm;
            case 4: return TextureFormat::rgba8_unorm;
            default: return TextureFormat::unknown;
        }
    }

    enum class TextureFlag : u8 {
        none = 0,
        has_transparency = 1 << 0,
        writable = 1 << 1,
        external = 1 << 2,
    };

    enum class TextureUsage : u16 {
        none = 0,
        sampled = 1 << 0,
        color_attachment = 1 << 1,
        depth_stencil_attachment = 1 << 2,
        transfer_source = 1 << 3,
        transfer_destination = 1 << 4,
    };

    constexpr TextureUsage operator|(
        const TextureUsage left,
        const TextureUsage right) noexcept {
        return static_cast<TextureUsage>(
            static_cast<u16>(left) | static_cast<u16>(right));
    }

    constexpr TextureUsage& operator|=(
        TextureUsage& left,
        const TextureUsage right) noexcept {
        left = left | right;
        return left;
    }

    [[nodiscard]] constexpr bool has_usage(
        const TextureUsage usages,
        const TextureUsage usage) noexcept {
        return (static_cast<u16>(usages) & static_cast<u16>(usage)) != 0;
    }

    [[nodiscard]] constexpr u8 texture_format_texel_size(
        const TextureFormat format) noexcept {
        switch (format) {
            case TextureFormat::r8_unorm: return 1;
            case TextureFormat::rg8_unorm: return 2;
            case TextureFormat::rgb8_unorm: return 3;
            case TextureFormat::rgba8_unorm:
            case TextureFormat::rgba8_srgb:
            case TextureFormat::bgra8_unorm:
            case TextureFormat::bgra8_srgb:
            case TextureFormat::r32_uint:
            case TextureFormat::depth32_float:
            case TextureFormat::depth24_unorm_stencil8: return 4;
            case TextureFormat::depth32_float_stencil8: return 8;
            case TextureFormat::unknown: return 0;
        }
        return 0;
    }

    constexpr TextureFlag operator|(
        const TextureFlag left,
        const TextureFlag right) noexcept {
        return static_cast<TextureFlag>(
            static_cast<u8>(left) | static_cast<u8>(right));
    }

    constexpr bool has_flag(
        const TextureFlag flags,
        const TextureFlag flag) noexcept {
        return (static_cast<u8>(flags) & static_cast<u8>(flag)) != 0;
    }

    struct TextureRegion {
        u32 x = 0;
        u32 y = 0;
        u32 width = 0;
        u32 height = 0;
    };

    struct Texture {
        u32 id = numeric::invalid_id;
        u32 width = 0;
        u32 height = 0;
        u8 channel_count = 0;
        TextureDimension dimension = TextureDimension::texture_2d;
        u8 layer_count = 1;
        TextureFormat format = TextureFormat::unknown;
        TextureSampleCount sample_count = TextureSampleCount::one;
        TextureFlag flags = TextureFlag::none;
        TextureUsage usage = TextureUsage::none;
        u32 generation = numeric::invalid_id;
        TextureState state = TextureState::unloaded;
        void* m_internal_data = nullptr;

        bool valid() const noexcept {
            return generation != numeric::invalid_id &&
                   m_internal_data != nullptr;
        }

        bool has_transparency() const noexcept {
            return has_flag(flags, TextureFlag::has_transparency);
        }

        bool writable() const noexcept {
            return has_flag(flags, TextureFlag::writable);
        }

        bool external() const noexcept {
            return has_flag(flags, TextureFlag::external);
        }
    };
}
