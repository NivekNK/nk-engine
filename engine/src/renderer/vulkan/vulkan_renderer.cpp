#include "nkpch.h"

#include "vulkan/vulkan_renderer.h"

#include "platform/platform.h"
#include "vulkan/utils.h"
#include "vulkan/resources/texture_data.h"

#include <glm/vertex_3d.h>

namespace nk {
    void VulkanRenderer::on_resized(u32 width, u32 height) {
        m_cached_framebuffer_width = width;
        m_cached_framebuffer_height = height;
        m_framebuffer_size_generation++;
        DebugLog("nk::VulkanRenderer::on_resized: {}, {}", width, height);
    }

    result<void, renderer_error> VulkanRenderer::init() {
        m_vulkan_allocator = nullptr;

        m_framebuffer_width = m_platform->width();
        m_framebuffer_height = m_platform->height();
        m_cached_framebuffer_width = m_framebuffer_width;
        m_cached_framebuffer_height = m_framebuffer_height;
        m_framebuffer_size_generation = 0;
        m_framebuffer_last_generation = 0;
        m_image_index = 0;
        m_current_frame = 0;

        m_instance.init(m_application_name.cstr(), m_platform, m_allocator, m_vulkan_allocator);
        m_instance_initialized = true;
        if (m_instance.get() == nullptr)
            return err(renderer_error{
                .code = renderer_error_code::initialization_failed,
                .native_code = 0,
            });

        m_device.init(m_platform, &m_instance, m_allocator, m_vulkan_allocator);
        m_device_initialized = true;
        if (m_device.get() == nullptr)
            return err(renderer_error{
                .code = renderer_error_code::initialization_failed,
                .native_code = 0,
            });

        auto swapchain_initialized = m_swapchain.init(
            m_framebuffer_width,
            m_framebuffer_height,
            &m_current_frame,
            &m_device,
            m_allocator,
            m_vulkan_allocator);
        m_swapchain_initialized = true;
        if (!swapchain_initialized)
            return err(swapchain_initialized.error());

        // clang-format off
        auto render_pass_initialized = m_main_render_pass.init(
            {
                .render_area = {{0, 0}, {m_framebuffer_width, m_framebuffer_height}},
                .clear_color = {0.0f, 0.0f, 0.45f, 1.0f},
                .depth = 1.0f,
                .stencil = 0,
            },
            m_swapchain, &m_device, m_vulkan_allocator
        );
        m_render_pass_initialized = true;
        if (!render_pass_initialized)
            return err(render_pass_initialized.error());
        // clang-format on

        const u32 image_count = m_swapchain.get_image_count();

        if (!m_framebuffers.dyarr_init_len(m_allocator, image_count, image_count))
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        auto framebuffers_created = recreate_framebuffers();
        if (!framebuffers_created)
            return err(framebuffers_created.error());
        InfoLog("Vulkan Framebuffers created ({}).", m_framebuffers.length());

        if (!m_graphics_command_buffers.dyarr_init_len(
                m_allocator, image_count, image_count))
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        auto command_buffers_created = recreate_command_buffers();
        if (!command_buffers_created)
            return err(command_buffers_created.error());
        InfoLog("Vulkan Command Buffers created ({}).", m_graphics_command_buffers.length());

        const u8 max_frames_in_flight = m_swapchain.get_max_frames_in_flight();
        if (!m_image_available_semaphores.dyarr_init_len(
                m_allocator, max_frames_in_flight, max_frames_in_flight) ||
            !m_queue_complete_semaphores.dyarr_init_len(
                m_allocator, max_frames_in_flight, max_frames_in_flight) ||
            !m_in_flight_fences.dyarr_init_len(
                m_allocator, max_frames_in_flight, max_frames_in_flight) ||
            !m_images_in_flight.dyarr_init_len(
                m_allocator, image_count, image_count)) {
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        }
        auto sync_created = recreate_sync_objects();
        if (!sync_created)
            return err(sync_created.error());
        InfoLog("Vulkan Sync Objects created.");

        auto shader_initialized = m_object_shader.init(
            m_framebuffer_width,
            m_framebuffer_height,
            image_count,
            &m_main_render_pass,
            &m_device,
            m_allocator,
            m_vulkan_allocator,
            &m_default_texture);
        if (!shader_initialized)
            return err(shader_initialized.error());
        InfoLog("Vulkan Object Shader created.");

        auto buffers_created = create_buffers();
        if (!buffers_created)
            return err(buffers_created.error());

        // TODO: temporary test code START
        constexpr u32 vertex_count = 4;
        glm::Vertex3D vertices[vertex_count];
        memset(vertices, 0, sizeof(glm::Vertex3D) * vertex_count);

        constexpr f32 f = 10.0f;

        vertices[0].position.x = -0.5f * f;
        vertices[0].position.y = -0.5f * f;
        vertices[0].texcoord.x = 0.0f;
        vertices[0].texcoord.y = 0.0f;

        vertices[1].position.y = 0.5f * f;
        vertices[1].position.x = 0.5f * f;
        vertices[1].texcoord.x = 1.0f;
        vertices[1].texcoord.y = 1.0f;

        vertices[2].position.x = -0.5f * f;
        vertices[2].position.y = 0.5f * f;
        vertices[2].texcoord.x = 0.0f;
        vertices[2].texcoord.y = 1.0f;

        vertices[3].position.x = 0.5f * f;
        vertices[3].position.y = -0.5f * f;
        vertices[3].texcoord.x = 1.0f;
        vertices[3].texcoord.y = 0.0f;

        constexpr u32 index_count = 6;
        u32 indices[index_count] = {0, 1, 2, 0, 3, 1};

        auto vertices_uploaded = upload_data_range(
            m_device.get_graphics_command_pool(),
            nullptr,
            m_device.get_graphics_queue(),
            &m_object_vertex_buffer,
            0,
            sizeof(glm::Vertex3D) * vertex_count,
            vertices);
        if (!vertices_uploaded)
            return err(vertices_uploaded.error());

        auto indices_uploaded = upload_data_range(
            m_device.get_graphics_command_pool(),
            nullptr,
            m_device.get_graphics_queue(),
            &m_object_index_buffer,
            0,
            sizeof(u32) * index_count,
            indices);
        if (!indices_uploaded)
            return err(indices_uploaded.error());

        u32 object_id = 0;
        if(!m_object_shader.acquire_resources(&object_id)) {
            return err(renderer_error{
                .code = renderer_error_code::object_resource_failed,
                .native_code = 0,
            });
        }
        // TODO: temporary test code END

        return ok();
    }

    void VulkanRenderer::shutdown() {
        if (m_device_initialized && m_device.get() != nullptr)
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

        if (m_render_pass_initialized) {
            m_main_render_pass.shutdown();
            m_render_pass_initialized = false;
        }
        if (m_swapchain_initialized) {
            m_swapchain.shutdown();
            m_swapchain_initialized = false;
        }
        if (m_device_initialized) {
            m_device.shutdown();
            m_device_initialized = false;
        }
        if (m_instance_initialized) {
            m_instance.shutdown();
            m_instance_initialized = false;
        }
    }

    result<frame_outcome, renderer_error> VulkanRenderer::begin_frame(
        const f64 delta_time) {
        m_frame_delta_time = delta_time;

        // Check if the framebuffer has been resized. If so, a new swapchain must be created
        if (m_framebuffer_size_generation != m_framebuffer_last_generation) {
            VkResult result = vkDeviceWaitIdle(m_device);
            if (!vk::is_success(result)) {
                return err(renderer_error{
                    .code = renderer_error_code::device_wait_failed,
                    .native_code = static_cast<i32>(result),
                });
            }

            auto recreated = recreate_swapchain();
            if (!recreated)
                return err(recreated.error());
            InfoLog("nk::VulkanRenderer::begin_frame Resized, booting.");
            return ok(frame_outcome::skipped_swapchain_recreation);
        }

        // Wait for the execution of the current frame to complete.
        // The fence being free will allow this one to move on.
        auto waited = m_in_flight_fences[m_current_frame].wait(numeric::u64_max);
        if (!waited)
            return err(waited.error());

        // Ensure the semaphore we're about to use is not still in use by a previous frame
        // This prevents the semaphore reuse issue that was causing the original errors
        const VkResult idle_result = vkDeviceWaitIdle(m_device);
        if (idle_result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::device_wait_failed,
                .native_code = static_cast<i32>(idle_result),
            });

        // Acquire the next image from the swap chain.
        // Pass along the semaphore that should signaled when this completes.
        // This same semaphore will later be waited on by the queue submission
        // to ensure this image is available.
        auto acquired = m_swapchain.acquire_next_image_index(
                &m_image_index,
                numeric::u64_max,
                m_image_available_semaphores[m_current_frame],
                nullptr);
        if (!acquired)
            return err(acquired.error());
        if (*acquired == swapchain_outcome::out_of_date) {
            ++m_framebuffer_size_generation;
            return ok(frame_outcome::skipped_swapchain_recreation);
        }
        if (*acquired == swapchain_outcome::suboptimal)
            ++m_framebuffer_size_generation;

        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];
        command_buffer.reset();
        auto command_begun = command_buffer.begin(false, false);
        if (!command_begun)
            return err(command_begun.error());

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

        return ok(frame_outcome::rendered);
    }

    result<frame_outcome, renderer_error> VulkanRenderer::end_frame(f64 delta_time) {
        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];

        // End renderpass
        m_main_render_pass.end(command_buffer);
        auto command_ended = command_buffer.end();
        if (!command_ended)
            return err(command_ended.error());

        // Make sure the previous frame is not using this image (i.e. its fence is being waited on)
        if (m_images_in_flight[m_image_index] != nullptr) { // was frame
            auto waited = m_images_in_flight[m_image_index]->wait(numeric::u64_max);
            if (!waited)
                return err(waited.error());
        }

        // Mark the image fence as in-use by this frame.
        m_images_in_flight[m_image_index] = &m_in_flight_fences[m_current_frame];

        // Reset the fence for use on the next frame
        auto reset = m_images_in_flight[m_image_index]->reset();
        if (!reset)
            return err(reset.error());

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
            return err(renderer_error{
                .code = renderer_error_code::queue_submit_failed,
                .native_code = static_cast<i32>(result),
            });
        }

        command_buffer.set_state(CommandBufferState::Submitted);
        // > End queue submission

        auto presented = m_swapchain.present(
            m_device.get_present_queue(),
            m_queue_complete_semaphores[m_current_frame],
            m_image_index);
        if (!presented)
            return err(presented.error());
        if (*presented != swapchain_outcome::ready)
            ++m_framebuffer_size_generation;

        if (*presented == swapchain_outcome::out_of_date)
            return ok(frame_outcome::skipped_swapchain_recreation);
        return ok(frame_outcome::rendered);
    }

    void VulkanRenderer::update_global_state(glm::mat4 projection, glm::mat4 view, glm::vec3 view_position, glm::vec4 ambient_color, i32 mode) {
        CommandBuffer* command_buffer = &m_graphics_command_buffers[m_image_index];
        m_object_shader.use(command_buffer);

        m_object_shader.set_global_ubo({
            .projection = projection,
            .view = view,
        });

        // TODO: Other ubo properties

        m_object_shader.update_global_state(m_graphics_command_buffers, m_image_index, m_frame_delta_time);
    }

    void VulkanRenderer::update_object(GeometryRenderData data) {
        CommandBuffer* command_buffer = &m_graphics_command_buffers[m_image_index];
        
        m_object_shader.update_object(m_graphics_command_buffers, m_image_index, data, m_frame_delta_time);

        // TODO: temporary test code START
        m_object_shader.use(command_buffer);

        // Bind the vertex buffer at offset.
        VkDeviceSize offsets[1] = {0};
        VkBuffer vertex_buffer = m_object_vertex_buffer.get();
        vkCmdBindVertexBuffers(command_buffer->get(), 0, 1, &vertex_buffer, static_cast<VkDeviceSize*>(offsets));
        
        // Bind index buffer at offset.
        vkCmdBindIndexBuffer(command_buffer->get(), m_object_index_buffer, 0, VK_INDEX_TYPE_UINT32);

        // Issue the draw.
        vkCmdDrawIndexed(command_buffer->get(), 6, 1, 0, 0, 0);
        // TODO: temporary test code END
    }

    result<void, renderer_error> VulkanRenderer::create_texture(
        strview name,
        bool auto_release,
        u32 width,
        u32 height,
        u32 channel_count,
        const u8* pixels,
        bool has_transparency,
        Texture* out_texture) {
        if (out_texture == nullptr || pixels == nullptr || width == 0 ||
            height == 0 || channel_count != 4)
            std::abort();

        Texture texture{};
        texture.width = width;
        texture.height = height;
        texture.channel_count = channel_count;
        texture.generation = 0;

        // TODO: Use an allocator for this.
        TextureData* texture_data = m_allocator->construct_t(TextureData);
        if (texture_data == nullptr)
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        texture.m_internal_data = texture_data;
        const VkDeviceSize image_size =
            static_cast<VkDeviceSize>(width) * height * channel_count;

        // NOTE: Assumes 8 bits per channel.
        VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

        // Create a staging buffer and load data into it.
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags memory_prop_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        Buffer staging;
        auto staging_initialized = staging.init(
            &m_device,
            m_vulkan_allocator,
            image_size,
            usage,
            memory_prop_flags,
            true);
        if (!staging_initialized) {
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(staging_initialized.error());
        }

        staging.load_data(0, image_size, 0, pixels);

        // NOTE: Lots of assumptions here, different texture types will require
        // different options here.
        auto image_initialized = texture_data->image.init(
            {
                .image_type = VK_IMAGE_TYPE_2D,
                .extent = {width, height},
                .format = image_format,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .create_view = true,
                .view_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
            },
            &m_device, m_vulkan_allocator);
        if (!image_initialized) {
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(image_initialized.error());
        }

        CommandBuffer temp_buffer;
        VkCommandPool pool = m_device.get_graphics_command_pool();
        VkQueue queue = m_device.get_graphics_queue();
        auto command_initialized = temp_buffer.init(pool, &m_device, true, true);
        if (!command_initialized) {
            texture_data->image.shutdown();
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(command_initialized.error());
        }

        // Transition the layout from whatever it is currently to optimal for recieving data.
        texture_data->image.transition_layout(
            &temp_buffer,
            image_format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Copy the data from the buffer.
        texture_data->image.copy_from_buffer(&temp_buffer, staging);

        // Transition from optimal for data reciept to shader-read-only optimal layout.
        texture_data->image.transition_layout(
            &temp_buffer,
            image_format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        auto command_ended = temp_buffer.end_single_use(queue);
        if (!command_ended) {
            texture_data->image.shutdown();
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(command_ended.error());
        }
        
        // Create a sampler for the texture
        VkSamplerCreateInfo sampler_info;
        memset(&sampler_info, 0, sizeof(VkSamplerCreateInfo));
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        // TODO: These filters should be configurable.
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = 16;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;

        VkResult result = vkCreateSampler(m_device, &sampler_info, m_vulkan_allocator, &texture_data->sampler);
        if (!vk::is_success(result)) {
            texture_data->image.shutdown();
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(renderer_error{
                .code = renderer_error_code::texture_sampler_creation_failed,
                .native_code = static_cast<i32>(result),
            });
        }

        texture.has_transparency = has_transparency;
        *out_texture = texture;
        return ok();
    }

    void VulkanRenderer::destroy_texture(Texture* texture) {
        if (texture == nullptr)
            std::abort();
        if (texture->m_internal_data == nullptr) {
            *texture = {};
            return;
        }
        vkDeviceWaitIdle(m_device);

        TextureData* texture_data = static_cast<TextureData*>(texture->m_internal_data);

        texture_data->image.shutdown();
        vkDestroySampler(m_device, texture_data->sampler, m_vulkan_allocator);
        texture_data->sampler = nullptr;

        m_allocator->deconstruct_t(TextureData, texture_data);
        *texture = {};
    }

    result<void, renderer_error> VulkanRenderer::recreate_framebuffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (image_count != m_framebuffers.length()) {
            if (!m_framebuffers.dyarr_resize(image_count))
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
        }

        for (u32 i = 0; i < m_framebuffers.length(); i++) {
            cl::arr<VkImageView> attachments;
            // clang-format off
            if (!attachments.arr_init_list(m_allocator, {
                m_swapchain.get_image_view_at(i),
                m_swapchain.get_depth_attachment()->get_view(),
            })) {
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
            }
            // clang-format on
            auto renewed = m_framebuffers[i].renew(
                m_framebuffer_width,
                m_framebuffer_height,
                attachments,
                &m_device,
                m_main_render_pass,
                m_vulkan_allocator);
            if (!renewed)
                return err(renewed.error());
        }
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::recreate_command_buffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (image_count != m_graphics_command_buffers.length()) {
            if (!m_graphics_command_buffers.dyarr_resize(image_count))
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
        }

        for (u32 i = 0; i < m_graphics_command_buffers.length(); i++) {
            auto renewed = m_graphics_command_buffers[i].renew(
                m_device.get_graphics_command_pool(), &m_device, true, false);
            if (!renewed)
                return err(renewed.error());
        }
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::recreate_sync_objects() {
        const u32 image_count = m_swapchain.get_image_count();
        const u8 max_frames_in_flight = m_swapchain.get_max_frames_in_flight();

        // Resize semaphore arrays to match image count
        if (max_frames_in_flight != m_image_available_semaphores.length()) {
            if (!m_image_available_semaphores.dyarr_resize(max_frames_in_flight) ||
                !m_queue_complete_semaphores.dyarr_resize(max_frames_in_flight)) {
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
            }
        }

        // Create semaphores for each image
        for (u32 i = 0; i < max_frames_in_flight; ++i) {
            VkSemaphoreCreateInfo semaphore_create_info = {};
            semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            
            if (m_image_available_semaphores[i] == nullptr) {
                const VkResult result = vkCreateSemaphore(
                    m_device,
                    &semaphore_create_info,
                    m_vulkan_allocator,
                    &m_image_available_semaphores[i]);
                if (result != VK_SUCCESS)
                    return err(renderer_error{
                        .code = renderer_error_code::initialization_failed,
                        .native_code = static_cast<i32>(result),
                    });
            }
            if (m_queue_complete_semaphores[i] == nullptr) {
                const VkResult result = vkCreateSemaphore(
                    m_device,
                    &semaphore_create_info,
                    m_vulkan_allocator,
                    &m_queue_complete_semaphores[i]);
                if (result != VK_SUCCESS)
                    return err(renderer_error{
                        .code = renderer_error_code::initialization_failed,
                        .native_code = static_cast<i32>(result),
                    });
            }
        }

        // Handle fences (still per frame)
        if (max_frames_in_flight != m_in_flight_fences.length()) {
            if (!m_in_flight_fences.dyarr_resize(max_frames_in_flight))
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
        }

        for (u8 i = 0; i < max_frames_in_flight; ++i) {
            auto renewed = m_in_flight_fences[i].renew(
                true, &m_device, m_vulkan_allocator);
            if (!renewed)
                return err(renewed.error());
        }

        // Handle images in flight
        if (image_count != m_images_in_flight.length()) {
            if (!m_images_in_flight.dyarr_resize(image_count))
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
        }
        std::memset(m_images_in_flight.data(), 0, sizeof(Fence*) * m_images_in_flight.length());
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::recreate_swapchain() {
        // Wait for any operations to complete.
        const VkResult wait_result = vkDeviceWaitIdle(m_device);
        if (wait_result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::device_wait_failed,
                .native_code = static_cast<i32>(wait_result),
            });

        auto swapchain_recreated = m_swapchain.recreate(
            m_cached_framebuffer_width, m_cached_framebuffer_height);
        if (!swapchain_recreated)
            return err(swapchain_recreated.error());

        auto sync_created = recreate_sync_objects();
        if (!sync_created)
            return err(sync_created.error());

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

        auto framebuffers_created = recreate_framebuffers();
        if (!framebuffers_created)
            return err(framebuffers_created.error());
        auto commands_created = recreate_command_buffers();
        if (!commands_created)
            return err(commands_created.error());
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::create_buffers() {
        VkMemoryPropertyFlags memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        constexpr u64 vertex_buffer_size = sizeof(glm::Vertex3D) * 1024 * 1024;
        auto vertex_buffer_initialized = m_object_vertex_buffer.init(
            &m_device,
            m_vulkan_allocator,
            vertex_buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            memory_property_flags,
            true);
        if (!vertex_buffer_initialized)
            return err(vertex_buffer_initialized.error());
        m_geometry_vertex_offset = 0;
        
        constexpr u64 index_buffer_size = sizeof(u32) * 1024 * 1024;
        auto index_buffer_initialized = m_object_index_buffer.init(
            &m_device,
            m_vulkan_allocator,
            index_buffer_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            memory_property_flags,
            true);
        if (!index_buffer_initialized)
            return err(index_buffer_initialized.error());
        m_geometry_index_offset = 0;

        InfoLog("Vulkan Object Buffers created.");
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::upload_data_range(
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
        auto staging_initialized = staging.init(
            &m_device,
            m_vulkan_allocator,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            flags,
            true);
        if (!staging_initialized)
            return err(staging_initialized.error());

        // Load the data into the staging buffer.
        staging.load_data(0, size, 0, data);

        // Perform the copy from staging to the device local buffer.
        auto copied = staging.copy_to({
            .pool = pool,
            .fence = fence,
            .queue = queue,
            .source = staging,
            .source_offset = 0,
            .destination = buffer->get(),
            .destination_offset = offset,
            .size = size,
        });
        if (!copied)
            return err(copied.error());

        // Clean up the staging buffer.
        staging.shutdown();
        return ok();
    }
}
