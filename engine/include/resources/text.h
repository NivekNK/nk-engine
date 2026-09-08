#pragma once

#include "collections/dyarr.h"
#include "core/strview.h"

namespace nk {
    class FontSystem;

    enum class text_error : u8 {
        invalid_configuration, not_initialized, already_initialized,
        out_of_memory, invalid_font, file_failed, font_failed,
        invalid_utf8, text_too_long, shaping_failed, atlas_full,
        raster_failed, renderer_failed,
    };

    struct FontHandle {
        const FontSystem* owner = nullptr;
        u32 index = numeric::invalid_id;
        u32 generation = 0;
        bool operator==(const FontHandle&) const noexcept = default;
    };

    enum class TextDirection : u8 { automatic, left_to_right, right_to_left };

    struct TextOptions {
        f32 size = 20.0f;             // Logical pixels (not points).
        f32 scale = 1.0f;             // Framebuffer pixels per logical pixel.
        TextDirection direction = TextDirection::automatic;
        strview language = "es";
        u32 script = 0;               // ISO 15924 tag; 0 asks HarfBuzz to guess.
        u8 tab_spaces = 4;
        bool ligatures = true;
    };

    struct TextMetrics {
        f32 width = 0;                // Advance extent, including whitespace.
        f32 height = 0;
        f32 ascender = 0;
        f32 descender = 0;
        f32 line_height = 0;
        f32 ink_left = 0;
        f32 ink_top = 0;
        f32 ink_right = 0;
        f32 ink_bottom = 0;
        u32 line_count = 0;
    };

    struct PositionedGlyph {
        u32 id = 0;                   // Font glyph id, never a Unicode codepoint.
        u32 cluster = 0;              // Byte offset in the original UTF-8 string.
        f32 x = 0;
        f32 baseline = 0;
        f32 advance = 0;
    };

    // Owns the shaped run, independent of Clay, Vulkan or source string lifetime.
    // Keep this value to avoid reshaping unchanged text each frame. The allocator
    // supplied to FontSystem must outlive layouts produced by that system.
    struct TextLayout {
        FontHandle font{};
        u32 pixel_size_64 = 0;
        f32 scale = 1;
        TextMetrics metrics{};
        cl::dyarr<PositionedGlyph> glyphs;
    };
}
