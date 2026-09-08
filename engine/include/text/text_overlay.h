#pragma once

#include "core/frame_metrics.h"
#include "renderer/text_renderer.h"

namespace nk {
    // Temporary diagnostic scene content, not a widget system or editor toolkit.
    class TextOverlay final {
    public:
        [[nodiscard]] result<void, text_error> init(mem::Allocator& allocator,
            Renderer& renderer, ShaderSystem& shaders, strview assets, f32 scale);
        [[nodiscard]] result<const TextFrame*, text_error> frame(u32 width, u32 height,
            f32 scale, f64 delta, const FrameMetricsSnapshot& metrics);
        void acknowledge_frame() { m_renderer.acknowledge_frame(); }
        FontStatistics statistics() const noexcept { return m_fonts.statistics(); }
    private:
        [[nodiscard]] result<void, text_error> rebuild(f32 scale);
        FontSystem m_fonts;
        TextRenderer m_renderer;
        TextDrawList m_list;
        TextLayout m_title, m_sample, m_help, m_status;
        FontHandle m_font;
        f32 m_scale = 0;
        f64 m_elapsed = 1;
    };
}
