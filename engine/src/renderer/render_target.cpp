#include "nkpch.h"

#include "renderer/render_target.h"

namespace nk {
    namespace {
        bool compatible_source(
            const RenderAttachmentConfig& config,
            const Texture& texture) noexcept {
            if (!texture.writable())
                return false;
            switch (config.source) {
                case RenderAttachmentSource::window_color:
                    return config.role == RenderAttachmentRole::color &&
                           texture.external();
                case RenderAttachmentSource::window_depth:
                    return config.role == RenderAttachmentRole::depth &&
                           !texture.external();
                case RenderAttachmentSource::texture:
                    return !texture.external();
            }
            return false;
        }
    }

    result<void, render_target_error> validate_render_pass_config(
        const RenderPassConfig& config) noexcept {
        if (config.name.empty())
            return err(render_target_error::invalid_name);
        if (config.area.width == 0 || config.area.height == 0)
            return err(render_target_error::invalid_area);
        if (config.attachments.empty() ||
            config.attachments.length() > max_render_target_attachments) {
            return err(render_target_error::invalid_attachment_count);
        }

        bool has_color = false;
        bool has_depth = false;
        const TextureSampleCount sample_count = config.attachments[0].sample_count;
        for (const RenderAttachmentConfig& attachment : config.attachments) {
            if (attachment.format == TextureFormat::unknown)
                return err(render_target_error::incompatible_format);
            const bool color = attachment.role == RenderAttachmentRole::color;
            if ((color && !is_color_format(attachment.format)) ||
                (!color && !is_depth_format(attachment.format))) {
                return err(render_target_error::incompatible_format);
            }
            if ((attachment.source == RenderAttachmentSource::window_color) != color ||
                (attachment.source == RenderAttachmentSource::window_depth && color)) {
                return err(render_target_error::incompatible_source);
            }
            if ((color && has_color) || (!color && has_depth))
                return err(render_target_error::duplicate_attachment_role);
            if (attachment.sample_count != sample_count)
                return err(render_target_error::incompatible_sample_count);
            has_color |= color;
            has_depth |= !color;
        }
        if (!has_color)
            return err(render_target_error::invalid_attachment);
        return ok();
    }

    result<void, render_target_error> RenderTarget::init(
        const RenderPassConfig& pass,
        const RenderTargetConfig& target) noexcept {
        auto pass_valid = validate_render_pass_config(pass);
        if (!pass_valid)
            return err(pass_valid.error());
        if (target.width == 0 || target.height == 0)
            return err(render_target_error::invalid_area);
        if (target.attachments.length() != pass.attachments.length() ||
            target.attachments.length() > max_render_target_attachments) {
            return err(render_target_error::invalid_attachment_count);
        }

        Texture* attachments[max_render_target_attachments]{};
        u32 generations[max_render_target_attachments]{};
        TextureFormat formats[max_render_target_attachments]{};
        TextureSampleCount sample_counts[max_render_target_attachments]{};
        RenderAttachmentRole roles[max_render_target_attachments]{};
        RenderAttachmentSource sources[max_render_target_attachments]{};
        for (u8 index = 0; index < target.attachments.length(); ++index) {
            Texture* texture = target.attachments[index];
            const RenderAttachmentConfig& expected = pass.attachments[index];
            if (texture == nullptr || !texture->valid())
                return err(render_target_error::invalid_attachment);
            if (!compatible_source(expected, *texture))
                return err(render_target_error::incompatible_source);
            if (texture->format != expected.format)
                return err(render_target_error::incompatible_format);
            if (texture->sample_count != expected.sample_count)
                return err(render_target_error::incompatible_sample_count);
            if (texture->width != target.width ||
                texture->height != target.height) {
                return err(render_target_error::incompatible_dimensions);
            }
            attachments[index] = texture;
            generations[index] = texture->generation;
            formats[index] = texture->format;
            sample_counts[index] = texture->sample_count;
            roles[index] = expected.role;
            sources[index] = expected.source;
        }

        reset();
        for (u8 index = 0; index < target.attachments.length(); ++index) {
            m_attachments[index] = attachments[index];
            m_generations[index] = generations[index];
            m_formats[index] = formats[index];
            m_sample_counts[index] = sample_counts[index];
            m_roles[index] = roles[index];
            m_sources[index] = sources[index];
        }
        m_width = target.width;
        m_height = target.height;
        m_attachment_count = static_cast<u8>(target.attachments.length());
        m_valid = true;
        return ok();
    }

    void RenderTarget::reset() noexcept {
        for (u8 index = 0; index < max_render_target_attachments; ++index) {
            m_attachments[index] = nullptr;
            m_generations[index] = numeric::invalid_id;
            m_formats[index] = TextureFormat::unknown;
            m_sample_counts[index] = TextureSampleCount::one;
            m_roles[index] = RenderAttachmentRole::color;
            m_sources[index] = RenderAttachmentSource::texture;
        }
        m_width = 0;
        m_height = 0;
        m_attachment_count = 0;
        m_valid = false;
    }

    bool RenderTarget::current() const noexcept {
        if (!m_valid)
            return false;
        for (u8 index = 0; index < m_attachment_count; ++index) {
            const Texture* texture = m_attachments[index];
            const RenderAttachmentConfig expected{
                .role = m_roles[index],
                .source = m_sources[index],
            };
            if (texture == nullptr || !texture->valid() ||
                !compatible_source(expected, *texture) ||
                texture->generation != m_generations[index] ||
                texture->format != m_formats[index] ||
                texture->sample_count != m_sample_counts[index] ||
                texture->width != m_width || texture->height != m_height) {
                return false;
            }
        }
        return true;
    }

    Texture* RenderTarget::attachment(
        const RenderAttachmentRole role) const noexcept {
        if (!m_valid)
            return nullptr;
        for (u8 index = 0; index < m_attachment_count; ++index) {
            if (m_roles[index] == role)
                return m_attachments[index];
        }
        return nullptr;
    }
}
