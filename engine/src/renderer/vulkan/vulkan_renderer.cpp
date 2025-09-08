#include "nkpch.h"

#include "vulkan/vulkan_renderer.h"

#include "platform/platform.h"
#include "vulkan/utils.h"

#include <glm/vertex_3d.h>

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

        const u8 max_frames_in_flight = m_swapchain.get_max_frames_in_flight();
        m_image_available_semaphores.dyarr_init_len(m_allocator, max_frames_in_flight, max_frames_in_flight);
        m_queue_complete_semaphores.dyarr_init_len(m_allocator, max_frames_in_flight, max_frames_in_flight);
        m_in_flight_fences.dyarr_init_len(m_allocator, max_frames_in_flight, max_frames_in_flight);
        m_images_in_flight.dyarr_init_len(m_allocator, image_count, image_count);
        recreate_sync_objects();
        InfoLog("Vulkan Sync Objects created.");

        m_object_shader.init(
            m_framebuffer_width,
            m_framebuffer_height,
            &m_main_render_pass,
            &m_device,
            m_vulkan_allocator);
        InfoLog("Vulkan Object Shader created.");

        create_buffers();

        // TODO: temporary test code START
        constexpr u32 vertex_count = 3;
        glm::Vertex3D vertices[vertex_count];
        memset(vertices, 0, sizeof(glm::Vertex3D) * vertex_count);

        vertices[0].position.x = 0.0f;
        vertices[0].position.y = -0.5f;

        vertices[1].position.x = 0.5f;
        vertices[1].position.y = 0.5f;

        vertices[2].position.x = 0.0f;
        vertices[2].position.y = 0.5f;

        constexpr u32 index_count = 3;
        u32 indices[index_count] = {0, 1, 2};

        upload_data_range(
            m_device.get_graphics_command_pool(),
            nullptr,
            m_device.get_graphics_queue(),
            &m_object_vertex_buffer,
            0,
            sizeof(glm::Vertex3D) * vertex_count,
            vertices);

        upload_data_range(
            m_device.get_graphics_command_pool(),
            nullptr,
            m_device.get_graphics_queue(),
            &m_object_index_buffer,
            0,
            sizeof(u32) * index_count,
            indices);
        // TODO: temporary test code END
    }

    void VulkanRenderer::shutdown() {
        vkDeviceWaitIdle(m_device);

        m_object_vertex_buffer.shutdown();
        m_object_index_buffer.shutdown();
        InfoLog("Vulkan Object Buffers shutdown.");

        m_object_shader.shutdown();
        InfoLog("Vulkan Object Shader shutdown.");

        // Clean up per-frame semaphores
        const u64 max_frames_in_flight = m_image_available_semaphores.length();
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
        // Check if the framebuffer has been resized. If so, a new swapchain must be created
        if (m_framebuffer_size_generation != m_framebuffer_last_generation) {
            VkResult result = vkDeviceWaitIdle(m_device);
            if (!vk::is_success(result)) {
                ErrorLog("nk::VulkanRenderer::begin_frame vkDeviceWaitIdle failed: '{}'.", vk::result_to_cstr(result, true));
                return false;
            }

            recreate_swapchain();
            InfoLog("nk::VulkanRenderer::begin_frame Resized, booting.");
            return false;
        }

        // Wait for the execution of the current frame to complete.
        // The fence being free will allow this one to move on.
        if (!m_in_flight_fences[m_current_frame].wait(numeric::u64_max)) {
            WarnLog("nk::VulkanRenderer::begin_frame In-flight fence wait failure!");
            return false;
        }

        // Ensure the semaphore we're about to use is not still in use by a previous frame
        // This prevents the semaphore reuse issue that was causing the original errors
        vkDeviceWaitIdle(m_device);

        // Acquire the next image from the swap chain.
        // Pass along the semaphore that should signaled when this completes.
        // This same semaphore will later be waited on by the queue submission
        // to ensure this image is available.
        if (!m_swapchain.acquire_next_image_index(
                &m_image_index,
                numeric::u64_max,
                m_image_available_semaphores[m_current_frame],
                nullptr)) {
            m_framebuffer_size_generation++;
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
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = m_framebuffer_width;
        scissor.extent.height = m_framebuffer_height;

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        m_main_render_pass.begin(command_buffer, m_framebuffers[m_image_index]);

        // TODO: temporary test code START
        m_object_shader.use(&command_buffer);

        // Bind the vertex buffer at offset.
        VkDeviceSize offsets[1] = {0};
        VkBuffer vertex_buffer = m_object_vertex_buffer.get();
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, static_cast<VkDeviceSize*>(offsets));
        
        // Bind index buffer at offset.
        vkCmdBindIndexBuffer(command_buffer, m_object_index_buffer, 0, VK_INDEX_TYPE_UINT32);

        // Issue the draw.
        vkCmdDrawIndexed(command_buffer, 3, 1, 0, 0, 0);
        // TODO: temporary test code END

        return true;
    }

    bool VulkanRenderer::end_frame(f64 delta_time) {
        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];

        // End renderpass
        m_main_render_pass.end(command_buffer);
        command_buffer.end();

        // Make sure the previous frame is not using this image (i.e. its fence is being waited on)
        if (m_images_in_flight[m_image_index] != nullptr) { // was frame
            m_images_in_flight[m_image_index]->wait(numeric::u64_max);
        }

        // Mark the image fence as in-use by this frame.
        m_images_in_flight[m_image_index] = &m_in_flight_fences[m_current_frame];

        // Reset the fence for use on the next frame
        m_images_in_flight[m_image_index]->reset();

        // Submit the queue and wait for the operation to complete.
        // > Begin queue submission
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Command buffer(s) to be executed.
        submit_info.commandBufferCount = 1;
        VkCommandBuffer p_command_buffer = command_buffer.get();
        submit_info.pCommandBuffers = &p_command_buffer;

        // The semaphore(s) to be signaled when the queue is complete.
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &m_queue_complete_semaphores[m_current_frame];

        // Wait semaphore ensures that the operation cannot begin until the image is available.
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &m_image_available_semaphores[m_current_frame];

        // Each semaphore waits on the corresponding pipeline stage to complete. 1:1 ratio.
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT prevents subsequent colour attachment
        // writes from executing until the semaphore signals (i.e. one frame is presented at a time)
        VkPipelineStageFlags flags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.pWaitDstStageMask = flags;

        VkResult result = vkQueueSubmit(
            m_device.get_graphics_queue(),
            1,
            &submit_info,
            m_in_flight_fences[m_current_frame]);
        if (result != VK_SUCCESS) {
            ErrorLog("nk::VulkanRenderer::end_frame vkQueueSubmit failed with result: '{}'.", vk::result_to_cstr(result, true));
            return false;
        }

        command_buffer.set_state(CommandBufferState::Submitted);
        // > End queue submission

        m_swapchain.present(
            m_device.get_present_queue(),
            m_queue_complete_semaphores[m_current_frame],
            m_image_index);

        m_frame_number++;

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
        const u32 image_count = m_swapchain.get_image_count();
        const u8 max_frames_in_flight = m_swapchain.get_max_frames_in_flight();

        // Resize semaphore arrays to match image count
        if (max_frames_in_flight != m_image_available_semaphores.length()) {
            m_image_available_semaphores.dyarr_resize(max_frames_in_flight);
            m_queue_complete_semaphores.dyarr_resize(max_frames_in_flight);
        }

        // Create semaphores for each image
        for (u32 i = 0; i < max_frames_in_flight; ++i) {
            VkSemaphoreCreateInfo semaphore_create_info = {};
            semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            
            if (m_image_available_semaphores[i] == nullptr) {
                vkCreateSemaphore(m_device, &semaphore_create_info, m_vulkan_allocator, &m_image_available_semaphores[i]);
            }
            if (m_queue_complete_semaphores[i] == nullptr) {
                vkCreateSemaphore(m_device, &semaphore_create_info, m_vulkan_allocator, &m_queue_complete_semaphores[i]);
            }
        }

        // Handle fences (still per frame)
        if (max_frames_in_flight != m_in_flight_fences.length()) {
            m_in_flight_fences.dyarr_resize(max_frames_in_flight);
        }

        for (u8 i = 0; i < max_frames_in_flight; ++i) {
            m_in_flight_fences[i].renew(true, &m_device, m_vulkan_allocator);
        }

        // Handle images in flight
        if (image_count != m_images_in_flight.length()) {
            m_images_in_flight.dyarr_resize(image_count);
        }
        std::memset(m_images_in_flight.data(), 0, sizeof(Fence*) * m_images_in_flight.length());
    }

    void VulkanRenderer::recreate_swapchain() {
        // Wait for any operations to complete.
        vkDeviceWaitIdle(m_device);

        m_swapchain.recreate(m_cached_framebuffer_width, m_cached_framebuffer_height);

        recreate_sync_objects();

        // Sync the framebuffer size with the cached sizes.
        m_framebuffer_width = m_cached_framebuffer_width;
        m_framebuffer_height = m_cached_framebuffer_height;
        m_cached_framebuffer_width = 0;
        m_cached_framebuffer_height = 0;

        // Update framebuffer size generation
        m_framebuffer_last_generation = m_framebuffer_size_generation;

        VkRect2D& render_area = m_main_render_pass.get_render_area();
        render_area.offset.x = 0;
        render_area.offset.y = 0;
        render_area.extent.width = m_framebuffer_width;
        render_area.extent.height = m_framebuffer_height;

        recreate_framebuffers();
        recreate_command_buffers();
    }

    void VulkanRenderer::create_buffers() {
        VkMemoryPropertyFlags memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        constexpr u64 vertex_buffer_size = sizeof(glm::Vertex3D) * 1024 * 1024;
        m_object_vertex_buffer.init(
            &m_device,
            m_vulkan_allocator,
            vertex_buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            memory_property_flags,
            true);
        m_geometry_vertex_offset = 0;
        

        constexpr u64 index_buffer_size = sizeof(u32) * 1024 * 1024;
        m_object_index_buffer.init(
            &m_device,
            m_vulkan_allocator,
            index_buffer_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            memory_property_flags,
            true);
        m_geometry_index_offset = 0;

        InfoLog("Vulkan Object Buffers created.");
    }

    void VulkanRenderer::upload_data_range(
        VkCommandPool pool,
        VkFence fence,
        VkQueue queue,
        Buffer* buffer,
        u64 offset,
        u64 size,
        void* data
    ) {
        // Create a host visible staging buffer to upload to mark is as the source of the transfer
        VkBufferUsageFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        Buffer staging;
        staging.init(
            &m_device,
            m_vulkan_allocator,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            flags,
            true);

        // Load the data into the staging buffer.
        staging.load_data(0, size, 0, data);

        // Perform the copy from staging to the device local buffer.
        staging.copy_to({
            .pool = pool,
            .fence = fence,
            .queue = queue,
            .source = staging,
            .source_offset = 0,
            .destination = buffer->get(),
            .destination_offset = offset,
            .size = size,
        });

        // Clean up the staging buffer.
        staging.shutdown();
    }
}
