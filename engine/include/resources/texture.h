#pragma once

#include "core/defines.h"

namespace nk {
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
