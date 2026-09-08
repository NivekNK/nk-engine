#include "nkpch.h"
#include "renderer/text_draw.h"
#include <cmath>

namespace nk {
    result<void, text_error> TextDrawList::init(mem::Allocator& allocator,
        FontSystem& fonts, u32 capacity, u32 max_glyphs) {
        if (m_fonts) return err(text_error::already_initialized);
        if (!capacity || max_glyphs < capacity || max_glyphs > 65536)
            return err(text_error::invalid_configuration);
        if (!m_vertices.dyarr_init(&allocator, static_cast<u64>(capacity) * 6) ||
            !m_batches.dyarr_init(&allocator, 64)) {
            shutdown(); return err(text_error::out_of_memory);
        }
        m_fonts = &fonts; m_max_glyphs = max_glyphs;
        return ok();
    }
    void TextDrawList::shutdown() noexcept {
        if (m_vertices.allocator()) (void)m_vertices.dyarr_shutdown();
        if (m_batches.allocator()) (void)m_batches.dyarr_shutdown();
        m_fonts = nullptr; m_width = m_height = 0;
    }
    result<void, text_error> TextDrawList::begin(u32 width, u32 height, f32 scale) {
        if (!m_fonts) return err(text_error::not_initialized);
        if (!width || !height || width > 65536 || height > 65536 ||
            !std::isfinite(scale) || scale < .25f || scale > 8)
            return err(text_error::invalid_configuration);
        (void)m_vertices.dyarr_reset(); (void)m_batches.dyarr_reset();
        m_width = width; m_height = height; m_scale = scale;
        return ok();
    }
    result<void, text_error> TextDrawList::append(const TextLayout& layout,
        glm::vec2 position, glm::vec4 color, TextClip clip,
        const PickId pick_id) {
        if (!m_fonts || !m_width) return err(text_error::not_initialized);
        if (!m_fonts->valid(layout.font)) return err(text_error::invalid_font);
        if (layout.scale != m_scale || !std::isfinite(position.x) || !std::isfinite(position.y) ||
            !std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.width) ||
            !std::isfinite(clip.height) || clip.width < 0 || clip.height < 0 ||
            !std::isfinite(color.r) || !std::isfinite(color.g) ||
            !std::isfinite(color.b) || !std::isfinite(color.a))
            return err(text_error::invalid_configuration);
        const f32 left = std::clamp(std::floor(clip.x * m_scale), 0.f, static_cast<f32>(m_width));
        const f32 top = std::clamp(std::floor(clip.y * m_scale), 0.f, static_cast<f32>(m_height));
        const f32 right = std::clamp(std::ceil((clip.x + clip.width) * m_scale), left, static_cast<f32>(m_width));
        const f32 bottom = std::clamp(std::ceil((clip.y + clip.height) * m_scale), top, static_cast<f32>(m_height));
        if (right == left || bottom == top || color.a <= 0) return ok();
        const TextureRegion scissor{static_cast<u32>(left), static_cast<u32>(top),
            static_cast<u32>(right - left), static_cast<u32>(bottom - top)};
        const u64 old_vertices = m_vertices.length(), old_batches = m_batches.length();
        if (layout.glyphs.length() > m_max_glyphs - old_vertices / 6)
            return err(text_error::text_too_long);
        if (!m_vertices.dyarr_reserve(old_vertices + layout.glyphs.length() * 6) ||
            !m_batches.dyarr_reserve(old_batches + layout.glyphs.length()))
            return err(text_error::out_of_memory);
        const u32 old_last_count = old_batches ? m_batches[old_batches - 1].vertex_count : 0;
        auto fail = [&](text_error error) -> result<void, text_error> {
            (void)m_vertices.dyarr_resize(old_vertices);
            (void)m_batches.dyarr_resize(old_batches);
            if (old_batches) m_batches[old_batches - 1].vertex_count = old_last_count;
            return err(error);
        };
        for (const auto& placed : layout.glyphs) {
            if (!std::isfinite(placed.x) || !std::isfinite(placed.baseline))
                return fail(text_error::invalid_configuration);
            auto raster = m_fonts->rasterize(layout, placed.id);
            if (!raster) return fail(raster.error());
            const auto& glyph = *raster;
            if (!glyph.width || !glyph.height) continue;
            const f32 x = (position.x + placed.x) * m_scale + glyph.bearing_x;
            const f32 y = (position.y + placed.baseline) * m_scale - glyph.bearing_y;
            if (x + glyph.width <= left || y + glyph.height <= top || x >= right || y >= bottom)
                continue;
            const auto page = m_fonts->page(glyph.page);
            const f32 u = static_cast<f32>(glyph.x) / page.width;
            const f32 v = static_cast<f32>(glyph.y) / page.height;
            const f32 u1 = static_cast<f32>(glyph.x + glyph.width) / page.width;
            const f32 v1 = static_cast<f32>(glyph.y + glyph.height) / page.height;
            const TextVertex vertices[]{
                {{x, y}, {u, v}, color}, {{x + glyph.width, y}, {u1, v}, color},
                {{x + glyph.width, y + glyph.height}, {u1, v1}, color},
                {{x, y}, {u, v}, color}, {{x + glyph.width, y + glyph.height}, {u1, v1}, color},
                {{x, y + glyph.height}, {u, v1}, color}};
            TextBatch* batch = m_batches.empty() ? nullptr : &m_batches[m_batches.length() - 1];
            if (!batch || batch->page != glyph.page ||
                batch->scissor.x != scissor.x || batch->scissor.y != scissor.y ||
                batch->scissor.width != scissor.width ||
                batch->scissor.height != scissor.height ||
                batch->pick_id != pick_id) {
                if (!m_batches.dyarr_emplace_back(TextBatch{glyph.page,
                    static_cast<u32>(m_vertices.length()), 0, scissor,
                    pick_id}))
                    return fail(text_error::out_of_memory);
                batch = &m_batches[m_batches.length() - 1];
            }
            for (const auto& vertex : vertices)
                if (!m_vertices.dyarr_emplace_back(vertex)) return fail(text_error::out_of_memory);
            batch->vertex_count += 6;
        }
        return ok();
    }
}
