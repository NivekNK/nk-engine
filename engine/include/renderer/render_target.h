#pragma once

#include <glm/ext/vector_float4.hpp>

#include "collections/slice.h"
#include "core/hash.h"
#include "core/result.h"
#include "core/strview.h"
#include "renderer/shader_config.h"
#include "resources/texture.h"

namespace nk {
    inline constexpr u8 max_render_target_attachments = 2;

    enum class RenderAttachmentRole : u8 {
        color,
        depth,
    };

    enum class RenderAttachmentSource : u8 {
        window_color,
        window_depth,
        texture,
    };

    enum class RenderLoadOperation : u8 {
        discard,
        load,
        clear,
    };

    enum class RenderStoreOperation : u8 {
        discard,
        store,
    };

    enum class RenderAttachmentUse : u8 {
        color_attachment,
        depth_stencil_attachment,
        present,
        sampled,
        transfer_source,
    };

    enum class RenderAttachmentResize : u8 {
        fixed,
        window,
    };

    enum class render_target_error : u8 {
        invalid_name,
        invalid_area,
        invalid_attachment_count,
        invalid_attachment,
        duplicate_attachment_role,
        incompatible_source,
        incompatible_format,
        incompatible_dimensions,
        incompatible_sample_count,
        incompatible_usage,
        incompatible_final_use,
        stale_attachment,
    };

    struct RenderArea {
        i32 x = 0;
        i32 y = 0;
        u32 width = 0;
        u32 height = 0;
    };

    struct RenderAttachmentConfig {
        RenderAttachmentRole role = RenderAttachmentRole::color;
        RenderAttachmentSource source = RenderAttachmentSource::texture;
        TextureFormat format = TextureFormat::unknown;
        TextureSampleCount sample_count = TextureSampleCount::one;
        RenderLoadOperation load = RenderLoadOperation::discard;
        RenderStoreOperation store = RenderStoreOperation::store;
        RenderAttachmentUse final_use = RenderAttachmentUse::color_attachment;
        RenderAttachmentResize resize = RenderAttachmentResize::fixed;
    };

    struct RenderPassSignature {
        TextureFormat color_format = TextureFormat::unknown;
        TextureFormat depth_stencil_format = TextureFormat::unknown;
        TextureSampleCount sample_count = TextureSampleCount::one;

        [[nodiscard]] bool valid() const noexcept {
            return is_color_format(color_format) &&
                (depth_stencil_format == TextureFormat::unknown ||
                 is_depth_format(depth_stencil_format));
        }

        bool operator==(const RenderPassSignature&) const noexcept = default;
    };

    [[nodiscard]] u64 hash64(
        const RenderPassSignature& signature,
        u64 seed = hash_seed::deterministic) noexcept;

    struct RenderPassConfig {
        strview name;
        RenderPassKind kind = RenderPassKind::world;
        RenderArea area;
        glm::vec4 clear_color{};
        f32 clear_depth = 1.0f;
        u32 clear_stencil = 0;
        cl::slice<const RenderAttachmentConfig> attachments;
        bool has_previous_pass = false;
        bool has_next_pass = false;
    };

    struct RenderTargetConfig {
        u32 width = 0;
        u32 height = 0;
        cl::slice<Texture* const> attachments;
    };

    [[nodiscard]] result<void, render_target_error>
    validate_render_pass_config(const RenderPassConfig& config) noexcept;
    [[nodiscard]] result<RenderPassSignature, render_target_error>
    render_pass_signature(const RenderPassConfig& config) noexcept;

    // Owns the built-in window pass descriptions. Vulkan resolves these
    // renderer-neutral descriptions into dynamic rendering or legacy render
    // passes; the backend does not define their attachment policy.
    class WindowRenderPasses final {
    public:
        [[nodiscard]] result<void, render_target_error> init(
            u32 width,
            u32 height,
            TextureFormat color_format,
            TextureFormat depth_format) noexcept;
        [[nodiscard]] result<void, render_target_error> resize(
            u32 width,
            u32 height) noexcept;

        [[nodiscard]] const RenderPassConfig& world() const noexcept {
            return m_world;
        }
        [[nodiscard]] const RenderPassConfig& ui() const noexcept {
            return m_ui;
        }

    private:
        RenderAttachmentConfig m_world_attachments[2]{};
        RenderAttachmentConfig m_ui_attachments[1]{};
        RenderPassConfig m_world{};
        RenderPassConfig m_ui{};
    };

    class RenderTarget final {
    public:
        [[nodiscard]] result<void, render_target_error> init(
            const RenderPassConfig& pass,
            const RenderTargetConfig& target) noexcept;
        void reset() noexcept;

        [[nodiscard]] bool valid() const noexcept { return m_valid; }
        [[nodiscard]] bool current() const noexcept;
        [[nodiscard]] u32 width() const noexcept { return m_width; }
        [[nodiscard]] u32 height() const noexcept { return m_height; }
        [[nodiscard]] u8 attachment_count() const noexcept {
            return m_attachment_count;
        }
        [[nodiscard]] Texture* attachment(const u8 index) const noexcept {
            return index < m_attachment_count ? m_attachments[index] : nullptr;
        }
        [[nodiscard]] Texture* attachment(
            RenderAttachmentRole role) const noexcept;

    private:
        Texture* m_attachments[max_render_target_attachments]{};
        u32 m_generations[max_render_target_attachments]{};
        TextureFormat m_formats[max_render_target_attachments]{};
        TextureSampleCount m_sample_counts[max_render_target_attachments]{};
        TextureUsage m_usages[max_render_target_attachments]{};
        RenderAttachmentRole m_roles[max_render_target_attachments]{};
        RenderAttachmentSource m_sources[max_render_target_attachments]{};
        u32 m_width = 0;
        u32 m_height = 0;
        u8 m_attachment_count = 0;
        bool m_valid = false;
    };
}
