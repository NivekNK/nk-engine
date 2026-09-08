#pragma once

#include "resources/text.h"
#include "resources/texture.h"

namespace nk {
    struct FontSystemConfig {
        u32 max_fonts = 8;
        u32 atlas_size = 1024;
        u32 max_atlas_pages = 8;
        u32 max_glyphs_per_font = 8192;
        u32 max_text_bytes = 65536;
    };

    struct RasterGlyph {
        u32 page = numeric::invalid_id;
        u32 x = 0, y = 0, width = 0, height = 0;
        i32 bearing_x = 0, bearing_y = 0;
    };

    struct FontAtlasPage {
        u32 width = 0, height = 0;
        cl::slice<const u8> pixels;
        TextureRegion dirty{};
        u64 revision = 0;
    };

    struct FontStatistics {
        u32 fonts = 0, pages = 0, rasterized_glyphs = 0;
        u64 shape_calls = 0, glyph_cache_hits = 0;
        u64 atlas_bytes = 0;
    };

    // Thread-confined CPU service. Worker rasterization can use separate instances;
    // sharing a mutable FT_Face across jobs is deliberately not part of the API.
    class FontSystem final {
    public:
        FontSystem() = default;
        ~FontSystem();
        FontSystem(const FontSystem&) = delete;
        FontSystem& operator=(const FontSystem&) = delete;

        [[nodiscard]] result<void, text_error> init(
            mem::Allocator& allocator, FontSystemConfig config = {});
        void shutdown() noexcept;
        [[nodiscard]] result<FontHandle, text_error> load(
            strview path, u32 face_index = 0);
        [[nodiscard]] result<void, text_error> unload(FontHandle font);
        [[nodiscard]] bool valid(FontHandle font) const noexcept;
        [[nodiscard]] result<void, text_error> layout(
            FontHandle font, strview text, const TextOptions& options,
            TextLayout& output);
        // Same shaping/metrics as layout(), but never rasterizes or uploads.
        [[nodiscard]] result<TextMetrics, text_error> measure(
            FontHandle font, strview text, const TextOptions& options = {});
        [[nodiscard]] result<RasterGlyph, text_error> rasterize(
            const TextLayout& layout, u32 glyph_id);
        [[nodiscard]] u32 page_count() const noexcept;
        [[nodiscard]] FontAtlasPage page(u32 index) const noexcept;
        void acknowledge_upload(u32 page, u64 revision) noexcept;
        [[nodiscard]] FontStatistics statistics() const noexcept;

    private:
        struct State;
        State* m_state = nullptr;
        mem::Allocator* m_allocator = nullptr;
        u32 m_generation = 0;
    };
}
