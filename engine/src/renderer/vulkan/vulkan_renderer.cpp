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

        const u32 image_count = m_swapchain.get_image_count();
        m_framebuffers.dyarr_init_len(m_allocator, image_count, image_count);
        recreate_framebuffers();
        InfoLog("Vulkan Framebuffers created ({}).", m_framebuffers.length());
    }

    void VulkanRenderer::shutdown() {
        m_framebuffers.dyarr_shutdown();
        InfoLog("Vulkan Framebuffers shutdown.");

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

    void VulkanRenderer::recreate_framebuffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (image_count != m_framebuffers.length()) {
            m_framebuffers.dyarr_resize(image_count);
        }

        for (u32 i = 0; i < m_framebuffers.length(); i++) {
            cl::arr<VkImageView> attachments;
            // clang-format off
            attachments.arr_init_list(m_allocator, {
                m_swapchain.get_image_view_at(i),
                m_swapchain.get_depth_attachment()->get_view(),
            });
            // clang-format on
            m_framebuffers[i].renew(
                m_framebuffer_width,
                m_framebuffer_height,
                attachments,
                &m_device,
                m_main_render_pass,
                m_vulkan_allocator);
        }
    }
}
