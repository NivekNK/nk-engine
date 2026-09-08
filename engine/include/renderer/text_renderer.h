#pragma once

#include "renderer/text_draw.h"
#include "renderer/shader.h"
#include "renderer/sampler.h"

namespace nk {
    class Renderer;
    class ShaderSystem;
    struct TextPageBinding {
        TextureBinding texture;
        u32 instance = numeric::invalid_id;
        u64 uploaded_revision = 0;
    };
    struct TextAtlasUpload {
        u32 page = 0;
        Texture* texture = nullptr;
        FontAtlasPage source;
    };
    // All slices are borrowed through draw_frame(); the backend copies host data
    // into the retired frame's mapped buffers before returning.
    struct TextFrame {
        cl::slice<const TextVertex> vertices;
        cl::slice<const TextBatch> batches;
        cl::slice<const TextPageBinding> pages;
        cl::slice<const TextAtlasUpload> uploads;
        ShaderHandle shader;
        ShaderUniformHandle projection_uniform, atlas_uniform;
        u32 width = 0, height = 0;
    };

    class TextRenderer final {
    public:
        TextRenderer() = default;
        ~TextRenderer() { shutdown(); }
        TextRenderer(const TextRenderer&) = delete;
        TextRenderer& operator=(const TextRenderer&) = delete;
        [[nodiscard]] result<void, text_error> init(mem::Allocator& allocator,
            FontSystem& fonts, Renderer& renderer, ShaderSystem& shaders);
        void shutdown() noexcept;
        [[nodiscard]] result<const TextFrame*, text_error> prepare(const TextDrawList& list);
        // Only after a successfully rendered frame. A skipped frame keeps dirty
        // regions pending, including atlas data discovered before resize.
        void acknowledge_frame() noexcept;
    private:
        FontSystem* m_fonts = nullptr;
        Renderer* m_renderer = nullptr;
        ShaderSystem* m_shaders = nullptr;
        ShaderHandle m_shader;
        ShaderUniformHandle m_projection_uniform, m_atlas_uniform;
        SamplerHandle m_sampler;
        cl::arr<Texture> m_textures;
        cl::arr<TextPageBinding> m_bindings;
        cl::dyarr<TextAtlasUpload> m_uploads;
        TextFrame m_frame;
    };
}
