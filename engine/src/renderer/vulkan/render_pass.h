#pragma once

#include "vulkan/vk.h"

namespace nk {
    class Device;
    class Swapchain;
    class CommandBuffer;

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

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;

        RenderPass(RenderPass&& other);
        RenderPass& operator=(RenderPass&& other);

        void init(const RenderPassCreateInfo& create_info,
                  Device& device,
                  Swapchain& swapchain,
                  VkAllocationCallbacks* allocator);
        void shutdown(Device& device);

        void begin(CommandBuffer& command_buffer, VkFramebuffer frame_buffer);
        void end(CommandBuffer& command_buffer);

        void set_render_area(const u16 x, const u16 y, const u16 width, const u16 height) { m_render_area = {x, y, width, height}; }
        void set_render_area_extent(const u16 width, const u16 height) { m_render_area.extent = {width, height}; }
        void set_render_area_offset(const u16 x, const u16 y) { m_render_area.offset = {x, y}; }

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
