#pragma once

#include "systems/font_system.h"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace nk {
    struct TextVertex { glm::vec2 position, uv; glm::vec4 color; };
    static_assert(sizeof(TextVertex) == 32);
    // Logical coordinates, origin at the top-left. Negative extents are invalid.
    struct TextClip { f32 x = 0, y = 0, width = 0, height = 0; };
    struct TextBatch {
        u32 page = 0, first_vertex = 0, vertex_count = 0;
        TextureRegion scissor{}; // Physical pixels, already intersected.
    };

    // Owned CPU geometry, borrowed font service. No UI or Vulkan dependency.
    // A frame preserves submission order; only adjacent compatible runs merge.
    class TextDrawList final {
    public:
        [[nodiscard]] result<void, text_error> init(mem::Allocator& allocator,
            FontSystem& fonts, u32 capacity = 1024, u32 max_glyphs = 65536);
        void shutdown() noexcept;
        [[nodiscard]] result<void, text_error> begin(u32 width, u32 height, f32 scale = 1);
        [[nodiscard]] result<void, text_error> append(const TextLayout& layout,
            glm::vec2 position, glm::vec4 color, TextClip clip);
        [[nodiscard]] result<void, text_error> append(const TextLayout& layout,
            glm::vec2 position, glm::vec4 color = glm::vec4{1}) {
            return append(layout, position, color, {0, 0, m_width / m_scale, m_height / m_scale});
        }
        cl::slice<const TextVertex> vertices() const noexcept { return cl::slice<const TextVertex>{m_vertices}; }
        cl::slice<const TextBatch> batches() const noexcept { return cl::slice<const TextBatch>{m_batches}; }
        u32 width() const noexcept { return m_width; }
        u32 height() const noexcept { return m_height; }
        const FontSystem* fonts() const noexcept { return m_fonts; }
    private:
        FontSystem* m_fonts = nullptr;
        cl::dyarr<TextVertex> m_vertices;
        cl::dyarr<TextBatch> m_batches;
        u32 m_width = 0, m_height = 0, m_max_glyphs = 0;
        f32 m_scale = 1;
    };
}
