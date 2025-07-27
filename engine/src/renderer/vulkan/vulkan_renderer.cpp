#include "nkpch.h"

#include "vulkan/vulkan_renderer.h"

#include "platform/platform.h"

namespace nk {
    void VulkanRenderer::on_resized(u32 width, u32 height) {
        DebugLog("nk::VulkanRenderer::on_resized: {}, {}", width, height);
    }

    void VulkanRenderer::init() {
        m_vulkan_allocator = nullptr;

        m_framebuffer_width = m_platform->width();
        m_framebuffer_height = m_platform->height();

        m_instance.init(m_application_name.c_str(), m_allocator, m_vulkan_allocator);
        m_device.init(m_platform, &m_instance, m_allocator, m_vulkan_allocator);
        m_swapchain.init(
            m_framebuffer_width,
            m_framebuffer_height,
            &m_current_frame,
            &m_device,
            m_allocator,
            m_vulkan_allocator);

        // clang-format off
        m_main_render_pass.init(
            {
                .render_area = {{0, 0}, {m_framebuffer_width, m_framebuffer_height}},
                .clear_color = {1.0f, 0.0f, 0.45f, 1.0f},
                .depth = 1.0f,
                .stencil = 0,
            },
            m_swapchain, &m_device, m_vulkan_allocator
        );
        // clang-format on
    }

    void VulkanRenderer::shutdown() {
        m_main_render_pass.shutdown();
        m_swapchain.shutdown();
        m_device.shutdown();
        m_instance.shutdown();
    }

    bool VulkanRenderer::begin_frame(f64 delta_time) {
        return true;
    }

    bool VulkanRenderer::end_frame(f64 delta_time) {
        return true;
    }
}
