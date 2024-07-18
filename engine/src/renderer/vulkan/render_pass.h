#pragma once

#include "vulkan/vk.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"

namespace nk {
    struct RenderPassColor {
        f32 r;
        f32 g;
        f32 b;
        f32 a;
    };

    struct RenderPassCreateInfo {
        VkRect2D render_area;
        RenderPassColor clear_color;
        f32 depth;
        u32 stencil;
    };

    class RenderPass {
    public:
        RenderPass() = default;

        void init(const RenderPassCreateInfo& create_info,
                  Device& device,
                  Swapchain& swapchain,
                  VkAllocationCallbacks* allocator);
        void shutdown(Device& device);

        VkRenderPass get() { return m_render_pass; }
        VkRenderPass operator()() { return m_render_pass; }
        operator VkRenderPass() { return m_render_pass; }

    private:
        VkAllocationCallbacks* m_allocator;

        VkRenderPass m_render_pass;

        VkRect2D m_render_area;
        RenderPassColor m_clear_color;
        f32 m_depth;
        u32 m_stencil;
    };
}
