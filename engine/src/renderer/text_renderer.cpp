#include "nkpch.h"
#include "renderer/text_renderer.h"
#include "renderer/renderer.h"
#include "systems/shader_system.h"

namespace nk {
    result<void, text_error> TextRenderer::init(mem::Allocator& allocator,
        FontSystem& fonts, Renderer& renderer, ShaderSystem& shaders) {
        if (m_fonts) return err(text_error::already_initialized);
        m_fonts = &fonts; m_renderer = &renderer; m_shaders = &shaders;
        if (!m_textures.arr_init(&allocator, 64) || !m_bindings.arr_init(&allocator, 64) ||
            !m_uploads.dyarr_init(&allocator, 64)) {
            shutdown(); return err(text_error::out_of_memory);
        }
        auto shader = shaders.load("Builtin.TextShader");
        if (!shader) { shutdown(); return err(text_error::renderer_failed); }
        m_shader = *shader;
        auto projection = shaders.uniform(m_shader, "projection");
        auto atlas = shaders.uniform(m_shader, "atlas");
        if (!projection || !atlas) { shutdown(); return err(text_error::renderer_failed); }
        m_projection_uniform = *projection; m_atlas_uniform = *atlas;
        SamplerConfig config;
        config.wrap_u = config.wrap_v = config.wrap_w = TextureWrap::clamp_to_edge;
        config.anisotropy = 1;
        auto sampler = renderer.create_sampler(config);
        if (!sampler) { shutdown(); return err(text_error::renderer_failed); }
        m_sampler = *sampler;
        return ok();
    }
    void TextRenderer::shutdown() noexcept {
        if (m_shader.valid()) {
            for (auto& binding : m_bindings)
                if (binding.instance != numeric::invalid_id)
                    (void)m_shaders->release_instance(m_shader, binding.instance);
            (void)m_shaders->destroy(m_shader);
        }
        for (auto& texture : m_textures)
            if (texture.m_internal_data) m_renderer->destroy_texture(&texture);
        if (m_sampler.valid()) (void)m_renderer->release_sampler(m_sampler);
        if (m_uploads.allocator()) (void)m_uploads.dyarr_shutdown();
        if (m_bindings.allocator()) (void)m_bindings.arr_shutdown();
        if (m_textures.allocator()) (void)m_textures.arr_shutdown();
        m_fonts = nullptr; m_renderer = nullptr; m_shaders = nullptr;
        m_shader = {}; m_sampler = {}; m_frame = {};
    }
    result<const TextFrame*, text_error> TextRenderer::prepare(const TextDrawList& list) {
        if (!m_fonts) return err(text_error::not_initialized);
        if (list.fonts() != m_fonts || !list.width() || !list.height())
            return err(text_error::invalid_configuration);
        (void)m_uploads.dyarr_clear();
        for (u32 index = 0; index < m_fonts->page_count(); ++index) {
            auto page = m_fonts->page(index);
            auto& texture = m_textures[index];
            auto& binding = m_bindings[index];
            if (!texture.valid()) {
                texture.width = page.width; texture.height = page.height;
                texture.channel_count = 1; texture.format = TextureFormat::r8_unorm;
                texture.flags = TextureFlag::writable | TextureFlag::has_transparency;
                auto created = m_renderer->create_writable_texture(&texture);
                if (!created) return err(text_error::renderer_failed);
                texture.generation = 0; texture.state = TextureState::ready;
            }
            if (binding.instance == numeric::invalid_id) {
                auto instance = m_shaders->acquire_instance(m_shader);
                if (!instance) return err(text_error::renderer_failed);
                binding = {{&texture, m_sampler}, *instance};
            }
            if (binding.uploaded_revision != page.revision &&
                (binding.uploaded_revision == 0 || page.dirty.width == 0))
                page.dirty = {0, 0, page.width, page.height};
            if (page.dirty.width && page.dirty.height &&
                !m_uploads.dyarr_emplace_back(TextAtlasUpload{index, &texture, page}))
                return err(text_error::out_of_memory);
        }
        m_frame = {list.vertices(), list.batches(),
            {m_bindings.data(), m_fonts->page_count()}, cl::slice<const TextAtlasUpload>{m_uploads}, m_shader,
            m_projection_uniform, m_atlas_uniform, list.width(), list.height()};
        return ok(&m_frame);
    }
    void TextRenderer::acknowledge_frame() noexcept {
        if (m_fonts)
            for (const auto& upload : m_uploads) {
                m_fonts->acknowledge_upload(upload.page, upload.source.revision);
                m_bindings[upload.page].uploaded_revision = upload.source.revision;
            }
    }
}
