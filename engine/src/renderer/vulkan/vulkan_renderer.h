#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"

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
        VkAllocationCallbacks* m_vulkan_allocator;
        Instance m_instance;
        Device m_device;
        Swapchain m_swapchain;
        RenderPass m_main_render_pass;

        u32 m_framebuffer_width;
        u32 m_framebuffer_height;

        u32 m_current_frame;
    };
}
