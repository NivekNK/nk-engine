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

        m_graphics_command_buffers.dyarr_init_len(m_allocator, image_count, image_count);
        recreate_command_buffers();
        InfoLog("Vulkan Command Buffers created ({}).", m_graphics_command_buffers.length());

        m_current_max_frames_in_flight = m_swapchain.get_max_frames_in_flight();
        m_image_available_semaphores.dyarr_init_len(m_allocator, m_current_max_frames_in_flight, m_current_max_frames_in_flight);
        m_queue_complete_semaphores.dyarr_init_len(m_allocator, m_current_max_frames_in_flight, m_current_max_frames_in_flight);
        m_in_flight_fences.dyarr_init_len(m_allocator, m_current_max_frames_in_flight, m_current_max_frames_in_flight);
        m_images_in_flight.dyarr_init_len(m_allocator, image_count, image_count);
        recreate_sync_objects();
        InfoLog("Vulkan Sync Objects created.");
    }

    void VulkanRenderer::shutdown() {
        const u64 max_frames_in_flight = MaxValue(
            m_image_available_semaphores.capacity(),
            m_queue_complete_semaphores.capacity());
        VkSemaphore* image_available_semaphores = m_image_available_semaphores.data();
        VkSemaphore* queue_complete_semaphores = m_queue_complete_semaphores.data();

        for (u64 i = 0; i < max_frames_in_flight; i++) {
            if (image_available_semaphores[i] != nullptr)
                vkDestroySemaphore(m_device, image_available_semaphores[i], m_vulkan_allocator);
            if (queue_complete_semaphores[i] != nullptr)
                vkDestroySemaphore(m_device, queue_complete_semaphores[i], m_vulkan_allocator);
        }

        m_image_available_semaphores.dyarr_shutdown();
        m_queue_complete_semaphores.dyarr_shutdown();
        m_in_flight_fences.dyarr_shutdown();
        m_images_in_flight.dyarr_shutdown();
        InfoLog("Vulkan Sync Objects shutdown.");

        m_graphics_command_buffers.dyarr_shutdown();
        InfoLog("Vulkan Command Buffers shutdown.");

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

    void VulkanRenderer::recreate_command_buffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (image_count != m_graphics_command_buffers.length()) {
            m_graphics_command_buffers.dyarr_resize(image_count);
        }

        for (u32 i = 0; i < m_graphics_command_buffers.length(); i++) {
            m_graphics_command_buffers[i].renew(m_device.get_graphics_command_pool(), &m_device, true, false);
        }
    }

    void VulkanRenderer::recreate_sync_objects() {
        const u8 max_frames_in_flight = m_swapchain.get_max_frames_in_flight();

        if (max_frames_in_flight != m_current_max_frames_in_flight) {
            m_image_available_semaphores.dyarr_resize(max_frames_in_flight);
            m_queue_complete_semaphores.dyarr_resize(max_frames_in_flight);
            m_in_flight_fences.dyarr_resize(max_frames_in_flight);
        }

        for (u8 i = 0; i < max_frames_in_flight; i++) {
            VkSemaphoreCreateInfo semaphore_create_info = {};
            semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            if (m_image_available_semaphores[i] == nullptr)
                vkCreateSemaphore(m_device, &semaphore_create_info, m_vulkan_allocator, &m_image_available_semaphores[i]);
            if (m_queue_complete_semaphores[i] == nullptr)
                vkCreateSemaphore(m_device, &semaphore_create_info, m_vulkan_allocator, &m_queue_complete_semaphores[i]);

            // Create the fence in a signaled state, indicating that the first frame has already been "rendered".
            // This will prevent the application from waiting indefinitely for the first frame to render since it
            // cannot be rendered until a frame is "rendered" before it.
            m_in_flight_fences[i].renew(true, &m_device, m_vulkan_allocator);
        }

        const u32 image_count = m_swapchain.get_image_count();
        if (image_count != m_images_in_flight.length()) {
            m_images_in_flight.dyarr_resize(image_count);
        }
        std::memset(m_images_in_flight.data(), 0, sizeof(Fence) * m_images_in_flight.length());

        m_current_max_frames_in_flight = max_frames_in_flight;
    }
}
