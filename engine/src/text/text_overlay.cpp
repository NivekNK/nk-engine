#include "nkpch.h"
#include "text/text_overlay.h"
#include "core/strbuf.h"
#include "core/format.h"

namespace nk {
    result<void, text_error> TextOverlay::init(mem::Allocator& allocator,
        Renderer& renderer, ShaderSystem& shaders, strview assets, f32 scale) {
        auto initialized = m_fonts.init(allocator);
        if (!initialized) return initialized;
        strbuf<1023> path{assets};
        if (!path.append("/fonts/NotoSans-Regular.ttf")) return err(text_error::file_failed);
        auto loaded = m_fonts.load(path.view());
        if (!loaded) return err(loaded.error());
        m_font = *loaded;
        initialized = m_renderer.init(allocator, m_fonts, renderer, shaders);
        if (!initialized) return initialized;
        initialized = m_list.init(allocator, m_fonts);
        if (!initialized) return initialized;
        return rebuild(scale);
    }
    result<void, text_error> TextOverlay::rebuild(f32 scale) {
        auto shaped = m_fonts.layout(m_font, "NK Engine — Texto", {.size=30, .scale=scale}, m_title);
        if (!shaped) return shaped;
        shaped = m_fonts.layout(m_font, "¡Hola! áéíóú ñ  •  office / affine\nFreeType + HarfBuzz · Atlas R8 · Slang",
            {.size=20, .scale=scale}, m_sample);
        if (!shaped) return shaped;
        shaped = m_fonts.layout(m_font, "WASD: cámara  |  0–2: render  |  ESC: salir",
            {.size=16, .scale=scale}, m_help);
        if (!shaped) return shaped;
        // Warm dynamic ASCII once per physical font size. Existing atlas cells
        // remain stable when the compositor later requests a different scale.
        char printable[95];
        for (u32 i = 0; i < 95; ++i) printable[i] = static_cast<char>(32 + i);
        TextLayout warm;
        shaped = m_fonts.layout(m_font, {printable, 95}, {.size=18, .scale=scale}, warm);
        if (!shaped) return shaped;
        for (const auto& glyph : warm.glyphs) {
            auto raster = m_fonts.rasterize(warm, glyph.id);
            if (!raster) return err(raster.error());
        }
        shaped = m_fonts.layout(m_font, "Preparando escena...", {.size=18, .scale=scale}, m_status);
        if (!shaped) return shaped;
        m_scale = scale; m_elapsed = 1;
        return ok();
    }
    result<const TextFrame*, text_error> TextOverlay::frame(u32 width, u32 height,
        f32 scale, f64 delta, const FrameMetricsSnapshot& metrics) {
        if (m_scale != scale) {
            auto rebuilt = rebuild(scale);
            if (!rebuilt) return err(rebuilt.error());
        }
        m_elapsed += delta;
        if (m_elapsed >= .25) {
            const auto stats = m_fonts.statistics();
            strbuf<255> status;
            (void)format_to(
                status,
                "{:.1f} ms | {:.0f} FPS | GPU {:.2f} ms | draws {} | visible {}/{} (-{}) | {} atlas | {} glyphs",
                metrics.average_frame_ms,
                metrics.average_fps,
                metrics.average_gpu_ms,
                metrics.latest.draw.draws,
                metrics.latest.draw.visible,
                metrics.latest.draw.candidates,
                metrics.latest.draw.culled,
                stats.pages,
                stats.rasterized_glyphs);
            auto shaped = m_fonts.layout(m_font, status.view(), {.size=18, .scale=scale}, m_status);
            if (!shaped) return err(shaped.error());
            m_elapsed = 0;
        }
        auto begun = m_list.begin(width, height, scale);
        if (!begun) return err(begun.error());
        const TextClip panel{20, 16, std::max(0.f, width / scale - 40.f), 210};
        auto line = [&](const TextLayout& layout, glm::vec2 position, glm::vec4 color)
            -> result<void, text_error> {
            auto shadow = m_list.append(layout, position + glm::vec2{1, 1}, {0, 0, 0, .9f}, panel);
            if (!shadow) return shadow;
            return m_list.append(layout, position, color, panel);
        };
        auto drawn = line(m_title, {24, 20}, {.45f, .92f, 1, 1});
        if (!drawn) return err(drawn.error());
        drawn = line(m_sample, {24, 69}, {1, 1, 1, 1});
        if (!drawn) return err(drawn.error());
        drawn = line(m_status, {24, 138}, {1, .85f, .45f, 1});
        if (!drawn) return err(drawn.error());
        drawn = line(m_help, {24, 176}, {.85f, .9f, .95f, 1});
        if (!drawn) return err(drawn.error());
        return m_renderer.prepare(m_list);
    }
}
