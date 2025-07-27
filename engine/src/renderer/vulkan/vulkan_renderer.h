#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "collections/dyarr.h"
#include "vulkan/framebuffer.h"
#include "vulkan/command_buffer.h"

namespace nk {
    class VulkanRenderer : public Renderer {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() = default;

        virtual void on_resized(u32 width, u32 height) override;

        virtual void init() override;
        virtual void shutdown() override;
        virtual bool begin_frame(f64 delta_time) override;
        virtual bool end_frame(f64 delta_time) override;

    private:
        void recreate_framebuffers();
        void recreate_command_buffers();

        VkAllocationCallbacks* m_vulkan_allocator;
        Instance m_instance;
        Device m_device;
        Swapchain m_swapchain;
        RenderPass m_main_render_pass;
        cl::dyarr<Framebuffer> m_framebuffers;
        cl::dyarr<CommandBuffer> m_graphics_command_buffers;

        u32 m_framebuffer_width;
        u32 m_framebuffer_height;

        u32 m_current_frame;
    };
}
