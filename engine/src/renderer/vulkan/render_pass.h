#pragma once

#include "vulkan/vk.h"
#include "glm/fwd.hpp"
#include "core/result.h"
#include "renderer/render_target.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Device;
    class CommandBuffer;
    class Framebuffer;

    class RenderPass {
    public:
        RenderPass() = default;
        ~RenderPass() = default;

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;
        RenderPass(RenderPass&&) = delete;
        RenderPass& operator=(RenderPass&&) = delete;
    
        [[nodiscard]] result<void, renderer_error> init(
            const RenderPassConfig& config,
            Device* device,
            VkAllocationCallbacks* vulkan_allocator);
        void shutdown();
        
        void begin(CommandBuffer& command_buffer, Framebuffer& frame_buffer);
        void end(CommandBuffer& command_buffer);

        VkRect2D& get_render_area() { return m_render_area; }
        VkFormat color_format() const noexcept { return m_color_format; }
        VkFormat depth_format() const noexcept { return m_depth_format; }
        const RenderPassSignature& signature() const noexcept {
            return m_signature;
        }
        const glm::vec4& clear_color() const noexcept { return m_clear_color; }
        const RenderAttachmentConfig* attachment(
            RenderAttachmentRole role) const noexcept;

        VkRenderPass get() { return m_render_pass; }
        VkRenderPass operator()() { return m_render_pass; }
        operator VkRenderPass() { return m_render_pass; }

    private:
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        Device* m_device = nullptr;

        VkRenderPass m_render_pass = nullptr;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        RenderPassSignature m_signature{};

        VkRect2D m_render_area{};
        glm::vec4 m_clear_color{};
        f32 m_depth = 0.0f;
        u32 m_stencil = 0;
        RenderAttachmentConfig
            m_attachments[max_render_target_attachments]{};
        u32 m_attachment_count = 0;
    };
}
