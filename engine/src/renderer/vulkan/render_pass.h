#pragma once

#include "vulkan/vk.h"
#include "glm/fwd.hpp"
#include "core/result.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Device;
    class Swapchain;
    class CommandBuffer;
    class Framebuffer;

    enum class RenderPassClearFlags : u8 {
        none = 0,
        color = 1 << 0,
        depth = 1 << 1,
        stencil = 1 << 2,
    };

    constexpr RenderPassClearFlags operator|(
        const RenderPassClearFlags left,
        const RenderPassClearFlags right) noexcept {
        return static_cast<RenderPassClearFlags>(
            static_cast<u8>(left) | static_cast<u8>(right));
    }

    constexpr bool has_flag(
        const RenderPassClearFlags flags,
        const RenderPassClearFlags flag) noexcept {
        return (static_cast<u8>(flags) & static_cast<u8>(flag)) != 0;
    }

    struct RenderPassCreateInfo {
        VkRect2D render_area;
        glm::vec4 clear_color;
        f32 depth;
        u32 stencil;
        RenderPassClearFlags clear_flags;
        bool has_previous_pass;
        bool has_next_pass;
        bool has_depth_attachment;
    };

    class RenderPass {
    public:
        RenderPass() = default;
        ~RenderPass() = default;

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;
        RenderPass(RenderPass&&) = delete;
        RenderPass& operator=(RenderPass&&) = delete;
    
        [[nodiscard]] result<void, renderer_error> init(
            const RenderPassCreateInfo& create_info,
            Swapchain& swapchain,
            Device* device,
            VkAllocationCallbacks* vulkan_allocator);
        void shutdown();
        
        void begin(CommandBuffer& command_buffer, Framebuffer& frame_buffer);
        void end(CommandBuffer& command_buffer);

        VkRect2D& get_render_area() { return m_render_area; }

        VkRenderPass get() { return m_render_pass; }
        VkRenderPass operator()() { return m_render_pass; }
        operator VkRenderPass() { return m_render_pass; }

    private:
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        Device* m_device = nullptr;

        VkRenderPass m_render_pass = nullptr;

        VkRect2D m_render_area{};
        glm::vec4 m_clear_color{};
        f32 m_depth = 0.0f;
        u32 m_stencil = 0;
        RenderPassClearFlags m_clear_flags = RenderPassClearFlags::none;
        u32 m_attachment_count = 0;
    };
}
