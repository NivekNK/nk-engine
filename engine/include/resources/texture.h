#pragma once

#include "core/defines.h"

namespace nk {
    enum class TextureFormat : u8 {
        unknown,
        r8_unorm,
        rg8_unorm,
        rgb8_unorm,
        rgba8_unorm,
        rgba8_srgb,
        bgra8_unorm,
        bgra8_srgb,
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
               format <= TextureFormat::bgra8_srgb;
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
        TextureFormat format = TextureFormat::unknown;
        TextureSampleCount sample_count = TextureSampleCount::one;
        TextureFlag flags = TextureFlag::none;
        u32 generation = numeric::invalid_id;
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
