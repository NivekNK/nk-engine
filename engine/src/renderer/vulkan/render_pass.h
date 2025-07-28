#pragma once

#include "vulkan/vk.h"
#include "glm/fwd.hpp"

namespace nk {
    class Device;
    class Swapchain;
    class CommandBuffer;
    class Framebuffer;

    struct RenderPassCreateInfo {
        VkRect2D render_area;
        glm::vec4 clear_color;
        f32 depth;
        u32 stencil;
    };

    class RenderPass {
    public:
        RenderPass() = default;
        ~RenderPass() = default;

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;
        RenderPass(RenderPass&&) = delete;
        RenderPass& operator=(RenderPass&&) = delete;
    
        void init(const RenderPassCreateInfo& create_info,
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
        VkAllocationCallbacks* m_vulkan_allocator;
        Device* m_device;

        VkRenderPass m_render_pass;

        VkRect2D m_render_area;
        glm::vec4 m_clear_color;
        f32 m_depth;
        u32 m_stencil;
    };
}
