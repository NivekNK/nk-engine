#include "nkpch.h"
#include "systems/font_system.h"
#include "resources/text_memory.h"
#include "core/utf8.h"
#include "collections/map.h"
#include "collections/arr.h"
#include "platform/file.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <hb.h>
#include <hb-ft.h>
#include <cmath>

namespace nk {
    namespace {
        struct alignas(std::max_align_t) FontBlock { u64 size; };
        struct FontMemory { mem::Allocator* allocator; bool failed = false; };
        constexpr FT_Int32 glyph_load_flags = FT_LOAD_DEFAULT |
            FT_LOAD_NO_BITMAP | FT_LOAD_NO_AUTOHINT;
        void* ft_allocate(FT_Memory memory, long size) {
            if (size <= 0 || static_cast<u64>(size) > SIZE_MAX - sizeof(FontBlock))
                return nullptr;
            auto& context = *static_cast<FontMemory*>(memory->user);
            auto& allocator = *context.allocator;
            auto* block = static_cast<FontBlock*>(allocator.allocate_raw(
                sizeof(FontBlock) + size, alignof(FontBlock)));
            if (block == nullptr) {
                context.failed = true;
                return nullptr;
            }
            block->size = size;
            return block + 1;
        }
        void ft_release(FT_Memory memory, void* pointer) {
            if (pointer != nullptr) {
                auto* block = static_cast<FontBlock*>(pointer) - 1;
                (void)static_cast<FontMemory*>(memory->user)->allocator->free_raw(
                    block, sizeof(FontBlock) + block->size);
            }
        }
        void* ft_reallocate(FT_Memory memory, long, long size, void* original) {
            if (size <= 0) { ft_release(memory, original); return nullptr; }
            void* replacement = ft_allocate(memory, size);
            if (replacement == nullptr)
                return nullptr;
            if (original != nullptr) {
                std::memcpy(replacement, original, std::min<u64>(size,
                    (static_cast<FontBlock*>(original) - 1)->size));
                ft_release(memory, original);
            }
            return replacement;
        }
        bool options_valid(const TextOptions& options) {
            const f64 physical = static_cast<f64>(options.size) * options.scale;
            return std::isfinite(physical) && options.size > 0 &&
                std::isfinite(options.scale) && options.scale >= 0.25f &&
                options.scale <= 8.0f && physical >= 1 && physical <= 512 &&
                options.tab_spaces >= 1 && options.tab_spaces <= 16 &&
                options.language.length() <= 63 &&
                options.direction <= TextDirection::right_to_left;
        }
    }

    struct FontSystem::State {
        struct Face {
            cl::dyarr<u8> bytes;
            cl::map<u64, RasterGlyph> glyphs;
            cl::map<u64, hb_glyph_extents_t> extents;
            FT_Face ft = nullptr;
            hb_font_t* hb = nullptr;
            u32 generation = 0;
            u32 physical_size = 0, shape_size = 0;
            bool set_size(u32 size) {
                if (physical_size == size) return true;
                if (FT_Set_Char_Size(ft, 0, size, 72, 72)) return false;
                physical_size = size;
                return true;
            }
            void reset() {
                if (hb) hb_font_destroy(hb);
                if (ft) FT_Done_Face(ft);
                hb = nullptr; ft = nullptr;
                if (glyphs.allocator()) (void)glyphs.map_shutdown();
                if (extents.allocator()) (void)extents.map_shutdown();
                physical_size = shape_size = 0;
                if (bytes.allocator()) (void)bytes.dyarr_shutdown();
            }
        };
        struct Page {
            cl::dyarr<u8> pixels;
            u32 cursor_x = 1, cursor_y = 1, row_height = 0;
            TextureRegion dirty{};
            u64 revision = 0;
        };
        FontSystemConfig config;
        FT_MemoryRec_ memory{};
        FontMemory memory_context{};
        FT_Library library = nullptr;
        hb_buffer_t* buffer = nullptr;
        cl::arr<Face> faces;
        cl::arr<Page> pages;
        TextLayout scratch;
        TextLayout measured;
        FontStatistics stats{};
    };

    FontSystem::~FontSystem() { shutdown(); }

    result<void, text_error> FontSystem::init(
        mem::Allocator& allocator, const FontSystemConfig config) {
        if (m_state) return err(text_error::already_initialized);
        if (!allocator.is_initialized() || config.max_fonts == 0 ||
            config.max_fonts > 256 || config.atlas_size < 32 || config.atlas_size > 4096 ||
            config.max_atlas_pages == 0 || config.max_atlas_pages > 64 ||
            config.max_glyphs_per_font == 0 || config.max_text_bytes == 0 ||
            config.max_text_bytes > 1024 * 1024)
            return err(text_error::invalid_configuration);
        m_allocator = &allocator;
        m_state = allocator.construct_t(State);
        if (!m_state) { m_allocator = nullptr; return err(text_error::out_of_memory); }
        m_state->config = config;
        m_state->memory_context.allocator = &allocator;
        m_state->memory = {&m_state->memory_context, ft_allocate, ft_release, ft_reallocate};
        if (FT_New_Library(&m_state->memory, &m_state->library) != 0) {
            shutdown(); return err(text_error::out_of_memory);
        }
        FT_Add_Default_Modules(m_state->library);
        if (m_state->memory_context.failed) {
            shutdown(); return err(text_error::out_of_memory);
        }
        // Reference this translation unit's allocator adapter also for static linking.
        (void)harfbuzz_memory_statistics();
        m_state->buffer = hb_buffer_create();
        if (!hb_buffer_allocation_successful(m_state->buffer) ||
            !m_state->faces.arr_init(&allocator, config.max_fonts) ||
            !m_state->pages.arr_init(&allocator, config.max_atlas_pages)) {
            shutdown(); return err(text_error::out_of_memory);
        }
        return ok();
    }

    void FontSystem::shutdown() noexcept {
        if (!m_state) return;
        for (auto& face : m_state->faces) face.reset();
        if (m_state->buffer) hb_buffer_destroy(m_state->buffer);
        if (m_state->library) FT_Done_Library(m_state->library);
        m_allocator->deconstruct_t(State, m_state);
        m_state = nullptr; m_allocator = nullptr;
    }

    bool FontSystem::valid(const FontHandle font) const noexcept {
        return m_state && font.owner == this && font.index < m_state->faces.length() &&
            m_state->faces[font.index].ft &&
            m_state->faces[font.index].generation == font.generation;
    }

    result<FontHandle, text_error> FontSystem::load(strview path, u32 face_index) {
        if (!m_state) return err(text_error::not_initialized);
        u32 slot = 0;
        while (slot < m_state->faces.length() && m_state->faces[slot].ft) ++slot;
        if (slot == m_state->faces.length()) return err(text_error::invalid_font);
        File file{*m_allocator};
        if (!file.open(path, FileMode::Read, true)) return err(text_error::file_failed);
        auto bytes = file.read_all_bytes(64 * 1024 * 1024);
        if (!bytes) return err(text_error::file_failed);
        auto& face = m_state->faces[slot];
        const FT_Error loaded = FT_New_Memory_Face(m_state->library,
            bytes->data(), static_cast<FT_Long>(bytes->length()), face_index, &face.ft);
        if (loaded != 0) return err(text_error::font_failed);
        if (!FT_IS_SCALABLE(face.ft) || FT_Select_Charmap(face.ft, FT_ENCODING_UNICODE) != 0) {
            face.reset(); return err(text_error::font_failed);
        }
        face.bytes = std::move(*bytes);
        face.hb = hb_ft_font_create_referenced(face.ft);
        if (face.hb == hb_font_get_empty() ||
            !face.glyphs.map_init(m_allocator, 256, hash_seed::deterministic) ||
            !face.extents.map_init(m_allocator, 256, hash_seed::deterministic)) {
            face.reset(); return err(text_error::out_of_memory);
        }
        hb_ft_font_set_load_flags(face.hb, glyph_load_flags);
        if (++m_generation == 0) ++m_generation;
        face.generation = m_generation;
        ++m_state->stats.fonts;
        return ok(FontHandle{this, slot, m_generation});
    }

    result<void, text_error> FontSystem::unload(const FontHandle font) {
        if (!valid(font)) return err(text_error::invalid_font);
        m_state->faces[font.index].reset();
        --m_state->stats.fonts;
        // Atlas cells stay stable while other layouts may be in use. Reclaim all
        // pages at shutdown; a future eviction policy must use explicit epochs.
        return ok();
    }

    result<void, text_error> FontSystem::layout(FontHandle font, strview text,
        const TextOptions& options, TextLayout& output) {
        if (!valid(font)) return err(text_error::invalid_font);
        if (!options_valid(options)) return err(text_error::invalid_configuration);
        if (output.glyphs.allocator() && output.glyphs.allocator() != m_allocator)
            return err(text_error::invalid_configuration);
        if (text.length() > m_state->config.max_text_bytes) return err(text_error::text_too_long);
        if (!utf8::valid(text)) return err(text_error::invalid_utf8);
        auto& scratch = m_state->scratch;
        if (!scratch.glyphs.allocator() && !scratch.glyphs.dyarr_init(m_allocator, 256))
            return err(text_error::out_of_memory);
        (void)scratch.glyphs.dyarr_clear();
        scratch.font = font;
        scratch.pixel_size_64 = static_cast<u32>(std::round(options.size * options.scale * 64));
        scratch.scale = options.scale;
        scratch.metrics = {};
        auto& face = m_state->faces[font.index];
        m_state->memory_context.failed = false;
        if (!face.set_size(scratch.pixel_size_64))
            return err(text_error::font_failed);
        if (face.shape_size != scratch.pixel_size_64) {
            hb_ft_font_changed(face.hb);
            face.shape_size = scratch.pixel_size_64;
        }
        auto& metrics = scratch.metrics;
        const f32 unit = 1.0f / (64 * options.scale);
        metrics.ascender = face.ft->size->metrics.ascender * unit;
        metrics.descender = -face.ft->size->metrics.descender * unit;
        metrics.line_height = face.ft->size->metrics.height * unit;
        metrics.line_count = text.empty() ? 0 : 1;
        bool have_ink = false;
        f32 pen_x = 0;
        f32 baseline = metrics.ascender;
        u32 offset = 0;
        const hb_language_t language = hb_language_from_string(
            options.language.data(), static_cast<int>(options.language.length()));
        while (offset < text.length()) {
            const char current = text[offset];
            if (current == '\n' || current == '\r') {
                if (current == '\r' && offset + 1 < text.length() && text[offset + 1] == '\n') ++offset;
                metrics.width = std::max(metrics.width, pen_x);
                pen_x = 0; baseline += metrics.line_height;
                ++metrics.line_count; ++offset; continue;
            }
            if (current == '\t') {
                hb_codepoint_t space = 0;
                (void)hb_font_get_nominal_glyph(face.hb, ' ', &space);
                const f32 tab = std::max(1.0f,
                    hb_font_get_glyph_h_advance(face.hb, space) * unit * options.tab_spaces);
                pen_x = (std::floor(pen_x / tab) + 1) * tab;
                ++offset; continue;
            }
            u32 end = offset;
            while (end < text.length() && text[end] != '\n' && text[end] != '\r' && text[end] != '\t') ++end;
            hb_buffer_t* buffer = m_state->buffer;
            hb_buffer_clear_contents(buffer);
            hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
            hb_buffer_set_language(buffer, language);
            if (options.script) hb_buffer_set_script(buffer, static_cast<hb_script_t>(options.script));
            if (options.direction != TextDirection::automatic)
                hb_buffer_set_direction(buffer, options.direction == TextDirection::left_to_right ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
            hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.length()), offset, end - offset);
            hb_buffer_guess_segment_properties(buffer);
            const hb_feature_t features[] = {
                {HB_TAG('l','i','g','a'), 0, 0, static_cast<unsigned>(-1)},
                {HB_TAG('c','l','i','g'), 0, 0, static_cast<unsigned>(-1)},
            };
            if (!hb_shape_full(face.hb, buffer, options.ligatures ? nullptr : features,
                options.ligatures ? 0 : 2, nullptr) || !hb_buffer_allocation_successful(buffer))
                return err(text_error::shaping_failed);
            if (m_state->memory_context.failed) return err(text_error::out_of_memory);
            unsigned count = 0;
            const auto* infos = hb_buffer_get_glyph_infos(buffer, &count);
            const auto* positions = hb_buffer_get_glyph_positions(buffer, nullptr);
            for (u32 i = 0; i < count; ++i) {
                PositionedGlyph glyph{infos[i].codepoint, infos[i].cluster,
                    pen_x + positions[i].x_offset * unit,
                    baseline - positions[i].y_offset * unit,
                    positions[i].x_advance * unit};
                if (!scratch.glyphs.dyarr_emplace_back(glyph)) return err(text_error::out_of_memory);
                hb_glyph_extents_t extent{};
                const u64 extent_key = (static_cast<u64>(scratch.pixel_size_64) << 32) | glyph.id;
                if (const auto* cached = face.extents.find(extent_key)) {
                    extent = *cached;
                } else {
                    (void)hb_font_get_glyph_extents(face.hb, glyph.id, &extent);
                    if (m_state->memory_context.failed) return err(text_error::out_of_memory);
                    if (face.extents.length() >= m_state->config.max_glyphs_per_font)
                        return err(text_error::atlas_full);
                    if (!face.extents.insert(extent_key, extent)) return err(text_error::out_of_memory);
                }
                if (extent.width != 0 && extent.height != 0) {
                    const f32 left = glyph.x + extent.x_bearing * unit;
                    const f32 top = glyph.baseline - extent.y_bearing * unit;
                    const f32 right = left + extent.width * unit;
                    const f32 bottom = top - extent.height * unit;
                    metrics.ink_left = have_ink ? std::min(metrics.ink_left, left) : left;
                    metrics.ink_top = have_ink ? std::min(metrics.ink_top, top) : top;
                    metrics.ink_right = have_ink ? std::max(metrics.ink_right, right) : right;
                    metrics.ink_bottom = have_ink ? std::max(metrics.ink_bottom, bottom) : bottom;
                    have_ink = true;
                }
                pen_x += glyph.advance;
            }
            offset = end;
        }
        metrics.width = std::max(metrics.width, pen_x);
        if (m_state->memory_context.failed) return err(text_error::out_of_memory);
        metrics.height = metrics.line_count * metrics.line_height;
        std::swap(output, scratch);
        ++m_state->stats.shape_calls;
        return ok();
    }

    result<TextMetrics, text_error> FontSystem::measure(FontHandle font,
        strview text, const TextOptions& options) {
        if (!m_state) return err(text_error::not_initialized);
        auto shaped = layout(font, text, options, m_state->measured);
        if (!shaped) return err(shaped.error());
        return ok(m_state->measured.metrics);
    }

    result<RasterGlyph, text_error> FontSystem::rasterize(
        const TextLayout& layout, const u32 glyph_id) {
        if (!valid(layout.font)) return err(text_error::invalid_font);
        if (layout.pixel_size_64 < 64 || layout.pixel_size_64 > 512 * 64)
            return err(text_error::invalid_configuration);
        auto& face = m_state->faces[layout.font.index];
        const u64 key = (static_cast<u64>(layout.pixel_size_64) << 32) | glyph_id;
        if (const auto* cached = face.glyphs.find(key)) {
            ++m_state->stats.glyph_cache_hits;
            return ok(*cached);
        }
        if (face.glyphs.length() >= m_state->config.max_glyphs_per_font)
            return err(text_error::atlas_full);
        if (!face.glyphs.reserve(face.glyphs.length() + 1)) return err(text_error::out_of_memory);
        if (!face.set_size(layout.pixel_size_64) ||
            FT_Load_Glyph(face.ft, glyph_id, glyph_load_flags) ||
            FT_Render_Glyph(face.ft->glyph, FT_RENDER_MODE_NORMAL))
            return err(text_error::raster_failed);
        const auto& bitmap = face.ft->glyph->bitmap;
        RasterGlyph glyph{numeric::invalid_id, 0, 0, bitmap.width, bitmap.rows,
            face.ft->glyph->bitmap_left, face.ft->glyph->bitmap_top};
        if (bitmap.width && bitmap.rows) {
            if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) return err(text_error::raster_failed);
            const u32 size = m_state->config.atlas_size;
            if (glyph.width + 2 > size || glyph.height + 2 > size) return err(text_error::atlas_full);
            u32 page_index = 0, x = 0, y = 0, row_height = 0;
            for (; page_index < m_state->config.max_atlas_pages; ++page_index) {
                auto& page = m_state->pages[page_index];
                x = page.cursor_x; y = page.cursor_y; row_height = page.row_height;
                if (x + glyph.width + 1 > size) {
                    x = 1; y += row_height + 1; row_height = 0;
                }
                if (y + glyph.height + 1 <= size) break;
            }
            if (page_index == m_state->config.max_atlas_pages) return err(text_error::atlas_full);
            auto& page = m_state->pages[page_index];
            if (!page.pixels.allocator()) {
                if (!page.pixels.dyarr_init_len(m_allocator,
                    static_cast<u64>(size) * size, static_cast<u64>(size) * size))
                    return err(text_error::out_of_memory);
                m_state->stats.pages = std::max(m_state->stats.pages, page_index + 1);
                m_state->stats.atlas_bytes += static_cast<u64>(size) * size;
            }
            glyph.page = page_index; glyph.x = x; glyph.y = y;
            for (u32 row = 0; row < glyph.height; ++row) {
                const u8* source = bitmap.pitch >= 0 ? bitmap.buffer + row * bitmap.pitch
                    : bitmap.buffer + (glyph.height - 1 - row) * (-bitmap.pitch);
                std::memcpy(page.pixels.data() + static_cast<u64>(y + row) * size + x,
                    source, glyph.width);
            }
            // Include untouched zero padding to prevent filtering neighbouring cells.
            const u32 min_x = page.dirty.width ? std::min(page.dirty.x, x - 1) : x - 1;
            const u32 min_y = page.dirty.height ? std::min(page.dirty.y, y - 1) : y - 1;
            const u32 max_x = std::max(page.dirty.x + page.dirty.width, x + glyph.width + 1);
            const u32 max_y = std::max(page.dirty.y + page.dirty.height, y + glyph.height + 1);
            page.dirty = {min_x, min_y, max_x - min_x, max_y - min_y};
            ++page.revision;
            page.cursor_x = x + glyph.width + 1;
            page.cursor_y = y;
            page.row_height = std::max(row_height, glyph.height);
        }
        if (!face.glyphs.insert(key, glyph)) return err(text_error::out_of_memory);
        ++m_state->stats.rasterized_glyphs;
        return ok(glyph);
    }

    u32 FontSystem::page_count() const noexcept { return m_state ? m_state->stats.pages : 0; }
    FontAtlasPage FontSystem::page(u32 index) const noexcept {
        if (index >= page_count()) return {};
        const auto& page = m_state->pages[index];
        return {m_state->config.atlas_size, m_state->config.atlas_size,
            cl::slice<const u8>{page.pixels}, page.dirty, page.revision};
    }
    void FontSystem::acknowledge_upload(u32 index, u64 revision) noexcept {
        if (index < page_count() && m_state->pages[index].revision == revision)
            m_state->pages[index].dirty = {};
    }
    FontStatistics FontSystem::statistics() const noexcept {
        return m_state ? m_state->stats : FontStatistics{};
    }
}
