#include "nkpch.h"

#include "vulkan/renderer_backend.h"

#include "nk/window.h"
#include "memory/malloc_allocator.h"

namespace nk {
    RendererBackend::RendererBackend(Window& window, str application_name)
        : m_window{window} {
        auto allocator = new MallocAllocator();
        allocator->allocator_init("VulkanRenderer", MemoryType::Renderer);
        m_allocator = allocator;

        m_instance.init(application_name.c_str(), m_allocator, m_vulkan_allocator);
        m_device.init(m_window, m_instance, m_allocator, m_vulkan_allocator);
        m_swapchain.init(window.width(), window.height(), &m_device, m_vulkan_allocator);

        auto main_render_pass_create_info = RenderPassCreateInfo {
            .render_area = {{0, 0}, {m_window.width(), m_window.height()}},
            .clear_color = {0.0f, 0.0f, 0.2f, 1.0f},
            .depth = 1.0f,
            .stencil = 0,
        };
        m_main_render_pass.init(main_render_pass_create_info, m_device, m_swapchain, m_vulkan_allocator);

        regenerate_framebuffers(m_window.width(), m_window.height());
        InfoLog("Vulkan Framebuffers created.");

        create_command_buffers();
        InfoLog("Vulkan Command Buffers created.");

        create_sync_objects();
        InfoLog("Vulkan Sync Objects created.");

        TraceLog("nk::RendererBackend created.");
    }

    RendererBackend::~RendererBackend() {
        vkDeviceWaitIdle(m_device);

        // Sync objects
        for (u32 i = 0; i < m_swapchain.get_max_frames_in_flight(); i++) {
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

        InfoLog("Vulkan Sync Objects destroyed.");

        m_graphics_command_buffers.clear();
        InfoLog("Vulkan Command Buffers destroyed.");

        m_framebuffers.clear();
        InfoLog("Vulkan Framebuffers destroyed.");

        m_main_render_pass.shutdown(m_device);

        m_swapchain.shutdown();

        m_device.shutdown(m_instance);

        m_instance.shutdown();

        delete m_allocator;
        TraceLog("nk::RendererBackend destroyed.");
    }

    bool RendererBackend::draw_frame(const RenderPacket& packet) {
        if (begin_frame(packet.delta_time)) {
            if (!end_frame(packet.delta_time)) {
                ErrorLog("Vulkan RendererBackend::draw_frame failed!");
                return false;
            }
        }

        return true;
    }

    bool RendererBackend::begin_frame(f64 delta_time) {
        // Check if recreating swap chain and boot out.
        if (m_recreating_swapchain) {
            VkResult result = vkDeviceWaitIdle(m_device);
            if (!vk::is_success(result)) {
                ErrorLog("RendererBackend::begin_frame vkDeviceWaitIdle (1) failed: '{}'.", vk::result_to_cstr(result, true));
                return false;
            }
            InfoLog("Recreating swapchain, booting.");
            return false;
        }

        if (m_window_was_resized) {
            VkResult result = vkDeviceWaitIdle(m_device);
            if (!vk::is_success(result)) {
                ErrorLog("RendererBackend::begin_frame vkDeviceWaitIdle (2) failed: '{}'.", vk::result_to_cstr(result, true));
                return false;
            }

            // If the swapchain recreation failed (because, for example, the window was minimized),
            // boot out before unsetting the flag.
            if (!recreate_swapchain())
                return false;

            m_window_was_resized = false;
            InfoLog("Resized, booting.");
            return false;
        }

        // Wait for the execution of the current frame to complete. The fence being free will allow this one to move on.
        const u32 current_frame = m_swapchain.get_current_frame();
        if (!m_in_flight_fences[current_frame].wait(u64_max)) {
            WarnLog("In-flight fence wait failure!");
            return false;
        }

        // Acquire the next image from the swap chain. Pass along the semaphore that should signaled when this completes.
        // This same semaphore will later be waited on by the queue submission to ensure this image is available.
        if (!m_swapchain.acquire_next_image_index(
            u64_max,
            m_image_available_semaphores[current_frame],
            nullptr,
            m_width,
            m_height,
            &m_image_index)) {
            return false;
        }

        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];
        command_buffer.reset();
        command_buffer.begin(false, false);

        // Dynamic state
        VkViewport viewport;
        viewport.x = 0.0f;
        viewport.y = static_cast<f32>(m_height);
        viewport.width = static_cast<f32>(m_width);
        viewport.height = -static_cast<f32>(m_height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // Scissor
        VkRect2D scissor;
        scissor.offset.x = scissor.offset.y = 0;
        scissor.extent.width = m_width;
        scissor.extent.height = m_height;

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        m_main_render_pass.set_render_area(m_width, m_height);
        m_main_render_pass.begin(command_buffer, m_framebuffers[m_image_index]);

        return true;
    }

    bool RendererBackend::end_frame(f64 delta_time) {
        m_frame_number++;
        return true;
    }

    void RendererBackend::on_window_resize(u16 width, u16 height) {
        m_width = width;
        m_height = height;
        m_window_was_resized = true;
    }

    void RendererBackend::regenerate_framebuffers(const u16 width, const u16 height) {
        const u32 image_count = m_swapchain.get_image_count();

        if (m_framebuffers.empty()) {
            m_framebuffers.init(m_allocator, image_count);

            u32 i = 0;
            for (auto& framebuffer : m_framebuffers) {
                // TODO: make this dynamic based on the currently configured attachments
                Arr<VkImageView> attachments;
                attachments.init_list(m_allocator, {m_swapchain.get_image_view_at(i), m_swapchain.get_depth_attachment().get_view()});
                framebuffer.init(width, height, attachments, &m_device, m_main_render_pass, m_vulkan_allocator);
                i++;
            }
        } else {
            const u32 old_image_count = m_framebuffers.length();

            if (image_count > old_image_count) {
                m_framebuffers.resize(image_count);
            }

            u32 i = 0;
            for (auto& framebuffer : m_framebuffers) {
                if (i < old_image_count) {
                    framebuffer.shutdown();
                }
                // TODO: make this dynamic based on the currently configured attachments
                Arr<VkImageView> attachments;
                attachments.init_list(m_allocator, {m_swapchain.get_image_view_at(i), m_swapchain.get_depth_attachment().get_view()});
                framebuffer.init(width, height, attachments, &m_device, m_main_render_pass, m_vulkan_allocator);
                i++;
            }
        }
    }

    void RendererBackend::create_command_buffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (m_graphics_command_buffers.empty()) {
            m_graphics_command_buffers.init(m_allocator, image_count);

            for (auto& command_buffer : m_graphics_command_buffers) {
                command_buffer.init(m_device.get_graphics_command_pool(), &m_device, true);
            }
        } else {
            const u32 old_image_count = m_graphics_command_buffers.length();

            if (image_count > old_image_count) {
                m_graphics_command_buffers.resize(image_count);
            }

            u32 i = 0;
            for (auto& command_buffer : m_graphics_command_buffers) {
                if (i < old_image_count) {
                    command_buffer.shutdown();
                }
                command_buffer.init(m_device.get_graphics_command_pool(), &m_device, true);
                i++;
            }
        }
    }

    void RendererBackend::create_sync_objects() {
        const u32 max_frames_in_flight = m_swapchain.get_max_frames_in_flight();

        m_image_available_semaphores.init(m_allocator, max_frames_in_flight);
        m_queue_complete_semaphores.init(m_allocator, max_frames_in_flight);
        m_in_flight_fences.init(m_allocator, max_frames_in_flight);

        for (u32 i = 0; i < max_frames_in_flight; ++i) {
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
    }

    bool RendererBackend::recreate_swapchain() {
        // If already being recreated, do not try again.
        if (m_recreating_swapchain) {
            InfoLog("RendererBackend::recreate_swapchain called when already recreating. Booting.");
            return false;
        }

        // Detect if the window is too small to be drawn to
        if (m_width == 0 || m_height == 0) {
            InfoLog("RendererBackend::recreate_swapchain called when window is < 1 in a dimension. Booting.");
            return false;
        }

        // Mark as recreating if the dimensions are valid.
        m_recreating_swapchain = true;

        // Wait for any operations to complete.
        vkDeviceWaitIdle(m_device);

        // Clear these out just in case.
        for (u32 i = 0; i < m_swapchain.get_image_count(); i++) {
            m_images_in_flight[i].shutdown();
        }

        m_device.detect_depth_format();
        m_swapchain.recreate(m_width, m_height);

        // Sync the framebuffer size with the cached sizes.
        m_main_render_pass.set_extent(m_width, m_height);

        // cleanup swapchain
        m_graphics_command_buffers.clear();

        m_main_render_pass.set_render_area(Rect2D{0, 0, m_width, m_height});

        // Framebuffers
        regenerate_framebuffers(m_width, m_height);

        create_command_buffers();

        // Clear the recreating flag.
        m_recreating_swapchain = false;

        return true;
    }
}