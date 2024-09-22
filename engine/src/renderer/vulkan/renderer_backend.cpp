#include "nkpch.h"

#include "vulkan/renderer_backend.h"

#include "nk/window.h"
#include "memory/malloc_allocator.h"

namespace nk {
    RendererBackend::RendererBackend(Window& window, cstr application_name)
        : m_window{window},
          m_frame_number{0} {
        auto allocator = new MallocAllocator();
        allocator->allocator_init("VulkanRenderer", MemoryType::Renderer);
        m_allocator = allocator;

        m_vulkan_allocator = nullptr;

        m_framebuffer_width = window.width() > 0 ? window.width() : 1280;
        m_framebuffer_height = window.height() > 0 ? window.height() : 720;
        m_cached_framebuffer_width = 0;
        m_cached_framebuffer_height = 0;

        m_instance.init(application_name, m_allocator, m_vulkan_allocator);
        m_device.init(m_window, &m_instance, m_allocator, m_vulkan_allocator);
        m_swapchain.init(m_framebuffer_width, m_framebuffer_height, &m_device, m_allocator, m_vulkan_allocator);

        RenderPassCreateInfo main_render_pass_create_info {
            .render_area = {{0, 0}, {m_framebuffer_width, m_framebuffer_height}},
            .clear_color = {1.0f, 0.0f, 0.45f, 1.0f},
            .depth = 1.0f,
            .stencil = 0,
        };
        m_main_render_pass.init(main_render_pass_create_info, m_device, m_swapchain, m_vulkan_allocator);

        recreate_framebuffers();
        InfoLog("Vulkan Framebuffers created.");

        recreate_command_buffers();
        InfoLog("Vulkan Command Buffers created.");

        create_sync_objects();

        TraceLog("nk::RendererBackend created.");
    }

    RendererBackend::~RendererBackend() {
        vkDeviceWaitIdle(m_device);

        // Sync objects
        for (u8 i = 0; i < m_swapchain.get_max_frames_in_flight(); i++) {
            if (m_image_available_semaphores[i]) {
                vkDestroySemaphore(m_device, m_image_available_semaphores[i], m_vulkan_allocator);
            }
            if (m_queue_complete_semaphores[i]) {
                vkDestroySemaphore(m_device, m_queue_complete_semaphores[i], m_vulkan_allocator);
            }
        }

        m_image_available_semaphores.clear();
        m_queue_complete_semaphores.clear();
        m_in_flight_fences.clear();
        m_images_in_flight.clear();
        InfoLog("Vulkan Sync Objects freed.");

        m_graphics_command_buffers.clear();
        InfoLog("Vulkan Command Buffers freed.");

        m_framebuffers.clear();
        InfoLog("Vulkan Framebuffers freed.");

        m_main_render_pass.shutdown(m_device);
        m_swapchain.shutdown();
        m_device.shutdown();
        m_instance.shutdown();

        m_vulkan_allocator = nullptr;

        delete m_allocator;
        TraceLog("nk::RendererBackend destroyed.");
    }

    bool RendererBackend::draw_frame(const RenderPacket& packet) {
        if (begin_frame(packet.delta_time)) {
            if (!end_frame(packet.delta_time)) {
                DebugLog("RendererBackend::end_frame failed. Shutting down application.");
                return false;
            }
        }

        return true;
    }

    bool RendererBackend::begin_frame(f64 delta_time) {
        return true;
    }

    bool RendererBackend::end_frame(f64 delta_time) {
        m_frame_number++;
        return true;
    }

    void RendererBackend::recreate_framebuffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (image_count > 0) {
            if (m_framebuffers.capacity() > 0) {
                if (m_framebuffers.length() != image_count)
                    m_framebuffers.resize(image_count);
            } else {
                m_framebuffers.init(m_allocator, image_count, image_count);
            }

            for (u32 i = 0; i < image_count; i++) {
                Arr<VkImageView> attachments;
                attachments.init_list(m_allocator, {m_swapchain.get_image_view_at(i), m_swapchain.get_depth_attachment().get_view()});

                Framebuffer& framebuffer = m_framebuffers[i];
                framebuffer.renew(m_framebuffer_width, m_framebuffer_height, attachments, &m_device, m_main_render_pass, m_vulkan_allocator);
            }
        } else {
            m_framebuffers.reset();
        }
    }

    void RendererBackend::recreate_command_buffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (image_count > 0) {
            if (m_graphics_command_buffers.capacity() > 0) {
                if (m_graphics_command_buffers.length() != image_count)
                    m_graphics_command_buffers.resize(image_count);
            } else {
                m_graphics_command_buffers.init(m_allocator, image_count, image_count);
            }

            for (auto& command_buffer : m_graphics_command_buffers) {
                command_buffer.renew(m_device.get_graphics_command_pool(), &m_device, true, false);
            }
        } else {
            m_graphics_command_buffers.reset();
        }
    }

    void RendererBackend::create_sync_objects() {
        const u8 max_frames_in_flight = m_swapchain.get_max_frames_in_flight();

        m_image_available_semaphores.init(m_allocator, max_frames_in_flight);
        m_queue_complete_semaphores.init(m_allocator, max_frames_in_flight);
        m_in_flight_fences.init(m_allocator, max_frames_in_flight);

        for (u8 i = 0; i < max_frames_in_flight; i++) {
            VkSemaphoreCreateInfo semaphore_create_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            vkCreateSemaphore(m_device, &semaphore_create_info, m_vulkan_allocator, &m_image_available_semaphores[i]);
            vkCreateSemaphore(m_device, &semaphore_create_info, m_vulkan_allocator, &m_queue_complete_semaphores[i]);

            // Create the fence in a signaled state, indicating that the first frame has already been "rendered".
            // This will prevent the application from waiting indefinitely for the first frame to render since it
            // cannot be rendered until a frame is "rendered" before it.
            m_in_flight_fences[i].init(true, &m_device, m_vulkan_allocator);
        }

        // In flight fences should not yet exist at this point, so clear the list. These are stored in pointers
        // because the initial state should be 0, and will be 0 when not in use. Acutal fences are not owned
        // by this list.
        m_images_in_flight.init(m_allocator, m_swapchain.get_image_count());
        InfoLog("Vulkan Sync Objects created.");
    }
}
