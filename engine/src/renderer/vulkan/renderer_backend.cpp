#include "nkpch.h"

#include "vulkan/renderer_backend.h"

#include "nk/window.h"
#include "memory/malloc_allocator.h"
#include "event/event.h"
#include "vulkan/utils.h"

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

        Event::WindowResize::add_listener([this](u16 width, u16 height) {
            this->on_window_resize(width, height);
        });

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
        // Check if recreating swap chain and boot out.
        if (m_recreating_swapchain) {
            VkResult result = vkDeviceWaitIdle(m_device);
            if (!vk::is_success(result)) {
                str result_str = vk::result_to_cstr(result, true);
                ErrorLog("RendererBackend::begin_frame vkDeviceWaitIdle (1) failed: '{}'.", result_str);
                return false;
            }
            InfoLog("RendererBackend::begin_frame Recreating swapchain, booting.");
            return false;
        }

        // Check if the framebuffer has been resized. If so, a new swapchain must be created
        if (m_framebuffer_size_generation != m_framebuffer_last_generation) {
            VkResult result = vkDeviceWaitIdle(m_device);
            if (!vk::is_success(result)) {
                str result_str = vk::result_to_cstr(result, true);
                ErrorLog("RendererBackend::begin_frame vkDeviceWaitIdle (2) failed: '{}'.", result_str);
                return false;
            }

            // If the swapchain recreation failed (because, for example, the window was minimized),
            // boot out before unsetting the flag.
            if (!recreate_swapchain()) {
                return false;
            }

            InfoLog("RendererBackend::begin_frame Resized, booting.");
            return false;
        }

        const u32& current_frame = m_swapchain.get_current_frame();

        // Wait for the execution of the current frame to complete.
        // The fence being free will allow this one to move on.
        if (!m_in_flight_fences[current_frame].wait(u64_max)) {
            WarnLog("RendererBackend::begin_frame In-flight fence wait failure!");
            return false;
        }

        // Acquire the next image from the swap chain.
        // Pass along the semaphore that should signaled when this completes.
        // This same semaphore will later be waited on by the queue submission
        // to ensure this image is available.
        if (!m_swapchain.acquire_next_image_index(
                m_framebuffer_width,
                m_framebuffer_height,
                u64_max,
                m_image_available_semaphores[current_frame],
                nullptr,
                &m_image_index)) {
            return false;
        }

        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];
        command_buffer.reset();
        command_buffer.begin(false, false);

        // Dynamic state
        VkViewport viewport;
        viewport.x = 0.0f;
        viewport.y = static_cast<f32>(m_framebuffer_height);
        viewport.width = static_cast<f32>(m_framebuffer_width);
        viewport.height = -static_cast<f32>(m_framebuffer_height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // Scissor
        VkRect2D scissor;
        scissor.offset.x = scissor.offset.y = 0;
        scissor.extent.width = m_framebuffer_width;
        scissor.extent.height = m_framebuffer_height;

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        m_main_render_pass.set_render_area_extent(m_framebuffer_width, m_framebuffer_height);
        m_main_render_pass.begin(command_buffer, m_framebuffers[m_image_index]);

        return true;
    }

    bool RendererBackend::end_frame(f64 delta_time) {
        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];

        // End renderpass
        m_main_render_pass.end(command_buffer);
        command_buffer.end();

        // Make sure the previous frame is not using this image (i.e. its fence is being waited on)
        if (m_images_in_flight[m_image_index] != nullptr) { // was frame
            m_images_in_flight[m_image_index]->wait(u64_max);
        }

        const u32& current_frame = m_swapchain.get_current_frame();

        // Mark the image fence as in-use by this frame.
        m_images_in_flight[m_image_index] = &m_in_flight_fences[current_frame];

        // Reset the fence for use on the next frame
        m_images_in_flight[m_image_index]->reset();

        // Submit the queue and wait for the operation to complete.
        // Begin queue submission
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

        // Command buffer(s) to be executed.
        submit_info.commandBufferCount = 1;
        VkCommandBuffer p_command_buffer = command_buffer;
        submit_info.pCommandBuffers = &p_command_buffer;

        // The semaphore(s) to be signaled when the queue is complete.
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &m_queue_complete_semaphores[current_frame];

        // Wait semaphore ensures that the operation cannot begin until the image is available.
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &m_image_available_semaphores[current_frame];

        // Each semaphore waits on the corresponding pipeline stage to complete. 1:1 ratio.
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT prevents subsequent colour attachment
        // writes from executing until the semaphore signals (i.e. one frame is presented at a time)
        VkPipelineStageFlags flags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.pWaitDstStageMask = flags;

        VkResult result = vkQueueSubmit(
            m_device.get_graphics_queue(),
            1,
            &submit_info,
            m_in_flight_fences[current_frame]);
        if (result != VK_SUCCESS) {
            str result_str = vk::result_to_cstr(result, true);
            ErrorLog("RendererBackend::end_frame vkQueueSubmit failed with result: '{}'.", result_str);
            return false;
        }

        command_buffer.set_state(CommandBufferState::Submitted);
        // End queue submission

        m_swapchain.present(
            m_framebuffer_width,
            m_framebuffer_height,
            m_device.get_graphics_queue(),
            m_device.get_present_queue(),
            m_queue_complete_semaphores[current_frame],
            m_image_index
        );

        m_frame_number++;
        return true;
    }

    void RendererBackend::on_window_resize(u16 width, u16 height) {
        m_cached_framebuffer_width = width;
        m_cached_framebuffer_height = height;
        m_framebuffer_size_generation++;

        DebugLog("RendererBackend::on_window_resize: {}, {}", width, height);
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

    bool RendererBackend::recreate_swapchain() {
        // If already being recreated, do not try again.
        if (m_recreating_swapchain) {
            InfoLog("RendererBackend::recreate_swapchain called when already recreating. Booting.");
            return false;
        }

        // Detect if the window is too small to be drawn to
        if (m_framebuffer_width == 0 || m_framebuffer_height == 0) {
            InfoLog("RendererBackend::recreate_swapchain called when window is < 1 in a dimension. Booting.");
            return false;
        }

        // Mark as recreating if the dimensions are valid.
        m_recreating_swapchain = true;

        // Wait for any operations to complete.
        vkDeviceWaitIdle(m_device);

        // Clear these out just in case.
        for (u32 i = 0; i < m_swapchain.get_image_count(); i++) {
            m_images_in_flight[i] = nullptr;
        }

        // Requery support
        m_device.query_swapchain_support_info();

        m_swapchain.recreate(m_cached_framebuffer_width, m_cached_framebuffer_height);

        // Sync the framebuffer size with the cached sizes.
        m_framebuffer_width = m_cached_framebuffer_width;
        m_framebuffer_height = m_cached_framebuffer_height;
        m_main_render_pass.set_render_area_extent(m_framebuffer_width, m_framebuffer_height);
        m_cached_framebuffer_width = 0;
        m_cached_framebuffer_height = 0;

        // Update framebuffer size generation
        m_framebuffer_last_generation = m_framebuffer_size_generation;

        m_main_render_pass.set_render_area(0, 0, m_framebuffer_width, m_framebuffer_height);

        recreate_framebuffers();
        recreate_command_buffers();

        // Clear the recreating flag.
        m_recreating_swapchain = false;

        return true;
    }
}
