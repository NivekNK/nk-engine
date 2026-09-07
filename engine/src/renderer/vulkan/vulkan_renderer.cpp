#include "nkpch.h"

#include "vulkan/vulkan_renderer.h"

#include "platform/platform.h"
#include "systems/resource_system.h"
#include "vulkan/utils.h"
#include "vulkan/resources/texture_data.h"
#include "vulkan/graphics_commands.h"

#include <glm/vertex_3d.h>
#include <glm/vertex_2d.h>

namespace nk {
    void VulkanRenderer::on_resized(u32 width, u32 height) {
        m_cached_framebuffer_width = width;
        m_cached_framebuffer_height = height;
        m_framebuffer_size_generation++;
        DebugLog("nk::VulkanRenderer::on_resized: {}, {}", width, height);
    }

    VulkanShader* VulkanRenderer::resolve_shader(
        const ShaderHandle handle) noexcept {
        if (!handle.valid() || handle.index >= max_shader_count)
            return nullptr;
        VulkanShaderSlot& slot = m_shaders[handle.index];
        if (slot.shader == nullptr || slot.generation != handle.generation)
            return nullptr;
        return slot.shader;
    }

    result<ShaderHandle, renderer_error> VulkanRenderer::create_shader(
        const ShaderConfig& config,
        const RenderPassKind render_pass) {
        if (m_allocator == nullptr || !m_device_initialized ||
            (render_pass == RenderPassKind::world &&
             !m_world_render_pass_initialized) ||
            (render_pass == RenderPassKind::ui &&
             !m_ui_render_pass_initialized)) {
            return err(renderer_error{
                renderer_error_code::shader_state_invalid,
                0,
            });
        }

        u16 slot_index = numeric::u16_max;
        for (u16 index = 0; index < max_shader_count; ++index) {
            if (m_shaders[index].shader == nullptr) {
                slot_index = index;
                break;
            }
        }
        if (slot_index == numeric::u16_max) {
            return err(renderer_error{
                renderer_error_code::shader_capacity_exceeded,
                0,
            });
        }

        VulkanShader* shader = m_allocator->construct_t(VulkanShader);
        if (shader == nullptr) {
            return err(renderer_error{
                renderer_error_code::out_of_memory,
                0,
            });
        }

        RenderPass* compatible_render_pass =
            render_pass == RenderPassKind::world
                ? &m_world_render_pass
                : &m_ui_render_pass;
        auto initialized = shader->init(
            config,
            m_framebuffer_width,
            m_framebuffer_height,
            m_swapchain.get_max_frames_in_flight(),
            compatible_render_pass,
            &m_device,
            m_allocator,
            m_resources,
            m_vulkan_allocator,
            m_default_texture);
        if (!initialized) {
            (void)m_allocator->deconstruct_t(VulkanShader, shader);
            return err(initialized.error());
        }

        VulkanShaderSlot& slot = m_shaders[slot_index];
        slot.shader = shader;
        slot.render_pass = render_pass;
        return ok(ShaderHandle{slot_index, slot.generation});
    }

    result<void, renderer_error> VulkanRenderer::destroy_shader(
        const ShaderHandle handle) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr)
            return err(renderer_error{
                renderer_error_code::shader_handle_invalid,
                0,
            });
        if (m_active_shader == handle && m_render_pass_active)
            return err(renderer_error{
                renderer_error_code::shader_state_invalid,
                0,
            });

        const VkResult waited = vkDeviceWaitIdle(m_device);
        if (waited != VK_SUCCESS) {
            return err(renderer_error{
                renderer_error_code::device_wait_failed,
                static_cast<i32>(waited),
            });
        }

        VulkanShaderSlot& slot = m_shaders[handle.index];
        (void)m_allocator->deconstruct_t(VulkanShader, shader);
        slot.shader = nullptr;
        slot.generation = slot.generation + 1 == numeric::u16_max
            ? 0
            : static_cast<u16>(slot.generation + 1);
        if (m_active_shader == handle)
            m_active_shader = {};
        return ok();
    }

    void VulkanRenderer::destroy_all_shaders() noexcept {
        for (VulkanShaderSlot& slot : m_shaders) {
            if (slot.shader == nullptr)
                continue;
            (void)m_allocator->deconstruct_t(VulkanShader, slot.shader);
            slot.shader = nullptr;
            slot.generation = slot.generation + 1 == numeric::u16_max
                ? 0
                : static_cast<u16>(slot.generation + 1);
        }
        m_active_shader = {};
    }

    result<void, renderer_error> VulkanRenderer::use_shader(
        const ShaderHandle handle) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr)
            return err(renderer_error{
                renderer_error_code::shader_handle_invalid,
                0,
            });
        if (!m_render_pass_active ||
            m_shaders[handle.index].render_pass != m_active_render_pass ||
            m_image_index >= m_graphics_command_buffers.length()) {
            return err(renderer_error{
                renderer_error_code::shader_state_invalid,
                0,
            });
        }
        auto used = shader->use(m_graphics_command_buffers[m_image_index]);
        if (!used)
            return err(used.error());
        m_active_shader = handle;
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::bind_shader_globals(
        const ShaderHandle handle) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr || handle != m_active_shader)
            return err(renderer_error{
                shader == nullptr
                    ? renderer_error_code::shader_handle_invalid
                    : renderer_error_code::shader_state_invalid,
                0,
            });
        return shader->bind_globals();
    }

    result<void, renderer_error> VulkanRenderer::bind_shader_instance(
        const ShaderHandle handle,
        const u32 instance_id) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr || handle != m_active_shader)
            return err(renderer_error{
                shader == nullptr
                    ? renderer_error_code::shader_handle_invalid
                    : renderer_error_code::shader_state_invalid,
                0,
            });
        return shader->bind_instance(instance_id);
    }

    result<void, renderer_error> VulkanRenderer::apply_shader_globals(
        const ShaderHandle handle) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr || handle != m_active_shader ||
            m_image_index >= m_graphics_command_buffers.length()) {
            return err(renderer_error{
                shader == nullptr
                    ? renderer_error_code::shader_handle_invalid
                    : renderer_error_code::shader_state_invalid,
                0,
            });
        }
        return shader->apply_globals(
            m_graphics_command_buffers[m_image_index], m_current_frame);
    }

    result<void, renderer_error> VulkanRenderer::apply_shader_instance(
        const ShaderHandle handle,
        const bool needs_update) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr || handle != m_active_shader ||
            m_image_index >= m_graphics_command_buffers.length()) {
            return err(renderer_error{
                shader == nullptr
                    ? renderer_error_code::shader_handle_invalid
                    : renderer_error_code::shader_state_invalid,
                0,
            });
        }
        return shader->apply_instance(
            m_graphics_command_buffers[m_image_index],
            m_current_frame,
            needs_update);
    }

    result<u32, renderer_error> VulkanRenderer::acquire_shader_instance(
        const ShaderHandle handle) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr)
            return err(renderer_error{
                renderer_error_code::shader_handle_invalid,
                0,
            });
        return shader->acquire_resources();
    }

    result<void, renderer_error> VulkanRenderer::release_shader_instance(
        const ShaderHandle handle,
        const u32 instance_id) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr)
            return err(renderer_error{
                renderer_error_code::shader_handle_invalid,
                0,
            });
        return shader->release_resources(instance_id);
    }

    result<void, renderer_error> VulkanRenderer::set_shader_uniform_raw(
        const ShaderHandle handle,
        const ShaderUniformHandle uniform,
        const ShaderUniformType type,
        const void* data,
        const u32 size) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr || handle != m_active_shader ||
            m_image_index >= m_graphics_command_buffers.length()) {
            return err(renderer_error{
                shader == nullptr
                    ? renderer_error_code::shader_handle_invalid
                    : renderer_error_code::shader_state_invalid,
                0,
            });
        }
        return shader->set_uniform(
            m_graphics_command_buffers[m_image_index],
            uniform,
            type,
            data,
            size);
    }

    result<void, renderer_error> VulkanRenderer::set_shader_sampler(
        const ShaderHandle handle,
        const ShaderUniformHandle uniform,
        Texture* texture,
        const u32 array_index) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr || handle != m_active_shader)
            return err(renderer_error{
                shader == nullptr
                    ? renderer_error_code::shader_handle_invalid
                    : renderer_error_code::shader_state_invalid,
                0,
            });
        return shader->set_sampler(uniform, texture, array_index);
    }

    void VulkanRenderer::on_default_texture_changed(Texture* texture) {
        for (VulkanShaderSlot& slot : m_shaders) {
            if (slot.shader != nullptr)
                slot.shader->set_default_texture(texture);
        }
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
        auto world_render_pass_initialized = m_world_render_pass.init(
            {
                .render_area = {{0, 0}, {m_framebuffer_width, m_framebuffer_height}},
                .clear_color = {0.0f, 0.0f, 0.45f, 1.0f},
                .depth = 1.0f,
                .stencil = 0,
                .clear_flags = RenderPassClearFlags::color |
                    RenderPassClearFlags::depth |
                    RenderPassClearFlags::stencil,
                .has_previous_pass = false,
                .has_next_pass = true,
                .has_depth_attachment = true,
            },
            m_swapchain, &m_device, m_vulkan_allocator
        );
        m_world_render_pass_initialized = true;
        if (!world_render_pass_initialized)
            return err(world_render_pass_initialized.error());

        auto ui_render_pass_initialized = m_ui_render_pass.init(
            {
                .render_area = {{0, 0}, {m_framebuffer_width, m_framebuffer_height}},
                .clear_color = glm::vec4(0.0f),
                .depth = 1.0f,
                .stencil = 0,
                .clear_flags = RenderPassClearFlags::none,
                .has_previous_pass = true,
                .has_next_pass = false,
                .has_depth_attachment = false,
            },
            m_swapchain, &m_device, m_vulkan_allocator
        );
        m_ui_render_pass_initialized = true;
        if (!ui_render_pass_initialized)
            return err(ui_render_pass_initialized.error());
        // clang-format on

        const u32 image_count = m_swapchain.get_image_count();

        if (!m_device.dynamic_rendering()) {
            if (!m_world_framebuffers.dyarr_init_len(
                    m_allocator, image_count, image_count) ||
                !m_ui_framebuffers.dyarr_init_len(
                    m_allocator, image_count, image_count))
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
            auto framebuffers_created = recreate_framebuffers();
            if (!framebuffers_created)
                return err(framebuffers_created.error());
            InfoLog(
                "Vulkan world/UI Framebuffers created ({} each).",
                m_world_framebuffers.length());
        }

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
                m_allocator, image_count, image_count) ||
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

        auto buffers_created = create_buffers();
        if (!buffers_created)
            return err(buffers_created.error());
        for (VulkanGeometryData& geometry : m_geometries)
            geometry.id = numeric::invalid_id;

        return ok();
    }

    void VulkanRenderer::shutdown() {
        if (m_device_initialized && m_device.get() != nullptr)
            vkDeviceWaitIdle(m_device);

        if (m_timestamp_pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(m_device, m_timestamp_pool, m_vulkan_allocator);
            m_timestamp_pool = VK_NULL_HANDLE;
        }
        m_timestamp_pending.arr_shutdown();

        m_object_vertex_buffer.shutdown();
        m_object_index_buffer.shutdown();
        InfoLog("Vulkan Object Buffers shutdown.");

        destroy_all_shaders();
        InfoLog("Vulkan shaders shutdown.");

        // Clean up per-frame semaphores
        const u64 max_frames_in_flight = m_image_available_semaphores.length();
        VkSemaphore* image_available_semaphores = m_image_available_semaphores.data();
        for (u64 i = 0; i < max_frames_in_flight; i++) {
            if (image_available_semaphores[i] != nullptr)
                vkDestroySemaphore(m_device, image_available_semaphores[i], m_vulkan_allocator);
        }
        for (VkSemaphore semaphore : m_queue_complete_semaphores)
            if (semaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(m_device, semaphore, m_vulkan_allocator);

        m_image_available_semaphores.dyarr_shutdown();
        m_queue_complete_semaphores.dyarr_shutdown();
        m_in_flight_fences.dyarr_shutdown();
        m_images_in_flight.dyarr_shutdown();
        InfoLog("Vulkan Sync Objects shutdown.");

        m_graphics_command_buffers.dyarr_shutdown();
        InfoLog("Vulkan Command Buffers shutdown.");

        m_ui_framebuffers.dyarr_shutdown();
        m_world_framebuffers.dyarr_shutdown();
        InfoLog("Vulkan world/UI Framebuffers shutdown.");

        if (m_ui_render_pass_initialized) {
            m_ui_render_pass.shutdown();
            m_ui_render_pass_initialized = false;
        }
        if (m_world_render_pass_initialized) {
            m_world_render_pass.shutdown();
            m_world_render_pass_initialized = false;
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

        // Retire the image's command buffer/depth storage before resetting or
        // recording it. Waiting in end_frame would already be too late.
        if (m_images_in_flight[m_image_index] != nullptr) {
            auto image_waited = m_images_in_flight[m_image_index]->wait(numeric::u64_max);
            if (!image_waited)
                return err(image_waited.error());
        }

        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];
        command_buffer.reset();
        auto command_begun = command_buffer.begin(false, false);
        if (!command_begun)
            return err(command_begun.error());

        m_gpu_frame_ms = -1.0;
        if (m_timestamp_pool != VK_NULL_HANDLE) {
            const u32 query = m_image_index * 2;
            if (m_timestamp_pending[m_image_index]) {
                u64 samples[4]{};
                const VkResult sampled = vkGetQueryPoolResults(
                    m_device, m_timestamp_pool, query, 2, sizeof(samples), samples,
                    sizeof(u64) * 2,
                    VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
                if (sampled == VK_SUCCESS && samples[1] && samples[3]) {
                    const u32 bits = m_device.timestamp_valid_bits();
                    const u64 mask = bits == 64 ? numeric::u64_max : (u64{1} << bits) - 1;
                    m_gpu_frame_ms = static_cast<f64>((samples[2] - samples[0]) & mask) *
                        m_device.timestamp_period() / 1000000.0;
                }
            }
            vkCmdResetQueryPool(command_buffer, m_timestamp_pool, query, 2);
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                m_timestamp_pool, query);
        }

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

        return ok(frame_outcome::rendered);
    }

    result<frame_outcome, renderer_error> VulkanRenderer::end_frame(f64) {
        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];

        if (m_timestamp_pool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                m_timestamp_pool, m_image_index * 2 + 1);
            m_timestamp_pending[m_image_index] = true;
        }

        auto command_ended = command_buffer.end();
        if (!command_ended)
            return err(command_ended.error());

        // Mark the image fence as in-use by this frame.
        m_images_in_flight[m_image_index] = &m_in_flight_fences[m_current_frame];

        // Reset the fence for use on the next frame
        auto reset = m_images_in_flight[m_image_index]->reset();
        if (!reset)
            return err(reset.error());

        auto submitted = m_device.submit(command_buffer,
            m_image_available_semaphores[m_current_frame],
            m_queue_complete_semaphores[m_image_index], m_in_flight_fences[m_current_frame]);
        if (!submitted)
            return err(submitted.error());

        command_buffer.set_state(CommandBufferState::Submitted);
        // > End queue submission

        auto presented = m_swapchain.present(
            m_device.get_present_queue(),
            m_queue_complete_semaphores[m_image_index],
            m_image_index);
        if (!presented)
            return err(presented.error());
        if (*presented != swapchain_outcome::ready)
            ++m_framebuffer_size_generation;

        if (*presented == swapchain_outcome::out_of_date)
            return ok(frame_outcome::skipped_swapchain_recreation);
        return ok(frame_outcome::rendered);
    }

    void VulkanRenderer::begin_render_pass(const RenderPassKind pass) {
        CommandBuffer& command_buffer =
            m_graphics_command_buffers[m_image_index];
        if (m_device.dynamic_rendering()) {
            const vk::GraphicsCommands commands{m_device, command_buffer};
            const bool world = pass == RenderPassKind::world;
            if (world) {
                commands.transition(m_swapchain.get_image_at(m_image_index),
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                    vk::ImageUse::discard, vk::ImageUse::color_attachment);
                VkImageAspectFlags depth_aspects = VK_IMAGE_ASPECT_DEPTH_BIT;
                if (m_device.get_depth_format() != VK_FORMAT_D32_SFLOAT)
                    depth_aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
                commands.transition(m_swapchain.get_depth_attachment(m_image_index)->get(),
                    {depth_aspects, 0, 1, 0, 1},
                    vk::ImageUse::discard, vk::ImageUse::depth_attachment);
            } else {
                commands.barrier(
                    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
                    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT});
            }
            const glm::vec4& clear = m_world_render_pass.clear_color();
            commands.begin_rendering({
                .color = m_swapchain.get_image_view_at(m_image_index),
                .depth = world ? m_swapchain.get_depth_attachment(m_image_index)->get_view() : VK_NULL_HANDLE,
                .area = {{0, 0}, {m_framebuffer_width, m_framebuffer_height}},
                .clear_color = {{clear.r, clear.g, clear.b, clear.a}},
                .clear = world,
            });
            command_buffer.set_state(CommandBufferState::InRenderPass);
        } else {
            switch (pass) {
                case RenderPassKind::world:
                    m_world_render_pass.begin(
                        command_buffer, m_world_framebuffers[m_image_index]);
                    break;
                case RenderPassKind::ui:
                    m_ui_render_pass.begin(
                        command_buffer, m_ui_framebuffers[m_image_index]);
                    break;
            }
        }
        m_active_shader = {};
        m_active_render_pass = pass;
        m_render_pass_active = true;
    }

    void VulkanRenderer::end_render_pass(const RenderPassKind pass) {
        CommandBuffer& command_buffer =
            m_graphics_command_buffers[m_image_index];
        if (m_device.dynamic_rendering()) {
            const vk::GraphicsCommands commands{m_device, command_buffer};
            commands.end_rendering();
            if (pass == RenderPassKind::ui) {
                commands.transition(m_swapchain.get_image_at(m_image_index),
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                    vk::ImageUse::color_attachment, vk::ImageUse::present);
            }
            command_buffer.set_state(CommandBufferState::Recording);
        } else {
            switch (pass) {
                case RenderPassKind::world:
                    m_world_render_pass.end(command_buffer);
                    break;
                case RenderPassKind::ui:
                    m_ui_render_pass.end(command_buffer);
                    break;
            }
        }
        m_active_shader = {};
        m_render_pass_active = false;
    }

    void VulkanRenderer::draw_geometry(
        const RenderPassKind pass,
        const GeometryRenderData data) {
        if (data.geometry == nullptr ||
            data.geometry->internal_id >= max_geometry_count) {
            return;
        }
        VulkanGeometryData& geometry =
            m_geometries[data.geometry->internal_id];
        if (geometry.id == numeric::invalid_id ||
            data.geometry->material == nullptr) {
            return;
        }

        const MaterialType expected_material_type =
            pass == RenderPassKind::world
                ? MaterialType::world
                : MaterialType::ui;
        if (data.geometry->material->type != expected_material_type) {
            ErrorLog("Geometry material type does not match the active render pass.");
            return;
        }

        const u64 expected_vertex_stride = pass == RenderPassKind::world
            ? sizeof(glm::Vertex3D)
            : sizeof(glm::Vertex2D);
        if (geometry.vertex_range.size !=
            geometry.vertex_count * expected_vertex_stride) {
            ErrorLog("Geometry vertex layout does not match the active render pass.");
            return;
        }

        CommandBuffer* command_buffer =
            &m_graphics_command_buffers[m_image_index];

        const vk::GraphicsCommands commands{m_device, command_buffer->get()};
        const vk::BufferView vertices{m_object_vertex_buffer.get(),
            geometry.vertex_range.offset, geometry.vertex_range.size};
        if (geometry.index_count != 0) {
            commands.draw_indexed(vertices,
                {m_object_index_buffer.get(), geometry.index_range.offset, geometry.index_range.size},
                static_cast<u32>(geometry.index_count));
        } else {
            commands.draw(vertices, static_cast<u32>(geometry.vertex_count));
        }
    }

    result<void, renderer_error> VulkanRenderer::create_texture(
        strview name,
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

        auto staged = staging.load_data(0, image_size, 0, pixels);
        if (!staged) {
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(staged.error());
        }

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

    result<void, renderer_error> VulkanRenderer::create_geometry(
        Geometry& geometry,
        const cl::slice<const glm::Vertex3D> vertices,
        const cl::slice<const u32> indices) {
        return create_geometry_internal(
            geometry,
            sizeof(glm::Vertex3D),
            alignof(glm::Vertex3D),
            vertices.length(),
            vertices.data(),
            indices);
    }

    result<void, renderer_error> VulkanRenderer::create_geometry(
        Geometry& geometry,
        const cl::slice<const glm::Vertex2D> vertices,
        const cl::slice<const u32> indices) {
        return create_geometry_internal(
            geometry,
            sizeof(glm::Vertex2D),
            alignof(glm::Vertex2D),
            vertices.length(),
            vertices.data(),
            indices);
    }

    result<void, renderer_error> VulkanRenderer::create_geometry_internal(
        Geometry& geometry,
        const u64 vertex_stride,
        const u64 vertex_alignment,
        const u64 vertex_count,
        const void* vertices,
        const cl::slice<const u32> indices) {
        if (vertices == nullptr || vertex_count == 0 ||
            vertex_count > numeric::u32_max ||
            indices.length() > numeric::u32_max) {
            return err(renderer_error{
                renderer_error_code::object_resource_failed,
                0,
            });
        }

        u32 internal_id = geometry.internal_id;
        if (internal_id == numeric::invalid_id) {
            for (u32 index = 0; index < max_geometry_count; ++index) {
                if (m_geometries[index].id == numeric::invalid_id) {
                    internal_id = index;
                    break;
                }
            }
        }
        if (internal_id >= max_geometry_count) {
            return err(renderer_error{
                renderer_error_code::object_resource_failed,
                0,
            });
        }

        if (vertex_stride == 0 || vertex_alignment == 0 ||
            vertex_count > numeric::u64_max / vertex_stride ||
            indices.length() > numeric::u64_max / sizeof(u32)) {
            return err(renderer_error{
                renderer_error_code::object_resource_failed,
                0,
            });
        }

        const u64 vertex_size = vertex_count * vertex_stride;
        const u64 index_size = indices.length() * sizeof(u32);

        auto vertex_reserved = m_object_vertex_buffer.reserve(
            vertex_size,
            vertex_alignment);
        if (!vertex_reserved)
            return err(vertex_reserved.error());

        mem::MemoryRange index_range{};
        if (!indices.empty()) {
            auto index_reserved = m_object_index_buffer.reserve(
                index_size,
                alignof(u32));
            if (!index_reserved) {
                auto vertex_released = m_object_vertex_buffer.release(
                    *vertex_reserved);
                if (!vertex_released)
                    ErrorLog("Failed to roll back a vertex buffer range.");
                return err(index_reserved.error());
            }
            index_range = *index_reserved;
        }

        auto vertices_uploaded = upload_data_range(
            m_device.get_graphics_command_pool(),
            nullptr,
            m_device.get_graphics_queue(),
            &m_object_vertex_buffer,
            vertex_reserved->offset,
            vertex_size,
            vertices);
        if (!vertices_uploaded) {
            if (index_range.size != 0) {
                auto index_released = m_object_index_buffer.release(index_range);
                if (!index_released)
                    ErrorLog("Failed to roll back an index buffer range.");
            }
            auto vertex_released = m_object_vertex_buffer.release(
                *vertex_reserved);
            if (!vertex_released)
                ErrorLog("Failed to roll back a vertex buffer range.");
            return err(vertices_uploaded.error());
        }

        if (!indices.empty()) {
            auto indices_uploaded = upload_data_range(
                m_device.get_graphics_command_pool(),
                nullptr,
                m_device.get_graphics_queue(),
                &m_object_index_buffer,
                index_range.offset,
                index_size,
                indices.data());
            if (!indices_uploaded) {
                auto index_released = m_object_index_buffer.release(index_range);
                auto vertex_released = m_object_vertex_buffer.release(
                    *vertex_reserved);
                if (!index_released || !vertex_released)
                    ErrorLog("Failed to roll back geometry buffer ranges.");
                return err(indices_uploaded.error());
            }
        }

        VulkanGeometryData uploaded{
            .id = internal_id,
            .generation = geometry.generation == numeric::invalid_id
                ? 0
                : geometry.generation + 1,
            .vertex_count = vertex_count,
            .vertex_range = *vertex_reserved,
            .index_count = indices.length(),
            .index_range = index_range,
        };
        if (uploaded.generation == numeric::invalid_id)
            uploaded.generation = 0;

        const VulkanGeometryData previous = m_geometries[internal_id];
        if (previous.id != numeric::invalid_id &&
            !release_geometry_ranges(previous)) {
            bool rollback_succeeded = true;
            if (index_range.size != 0) {
                auto index_released = m_object_index_buffer.release(
                    index_range);
                rollback_succeeded = static_cast<bool>(index_released);
            }
            auto vertex_released = m_object_vertex_buffer.release(
                *vertex_reserved);
            if (!rollback_succeeded || !vertex_released)
                ErrorLog("Failed to roll back replacement geometry ranges.");
            return err(renderer_error{
                renderer_error_code::buffer_release_failed,
                0,
            });
        }

        m_geometries[internal_id] = uploaded;
        geometry.internal_id = internal_id;
        geometry.generation = uploaded.generation;
        return ok();
    }

    void VulkanRenderer::destroy_geometry(Geometry& geometry) {
        if (geometry.internal_id >= max_geometry_count)
            return;

        VulkanGeometryData& internal = m_geometries[geometry.internal_id];
        if (internal.id == numeric::invalid_id)
            return;

        const VkResult wait_result = vkQueueWaitIdle(
            m_device.get_graphics_queue());
        if (wait_result != VK_SUCCESS) {
            ErrorLog("Geometry destruction could not wait for the graphics queue.");
            return;
        }
        if (!release_geometry_ranges(internal)) {
            ErrorLog("Geometry destruction could not release its buffer ranges.");
            return;
        }

        m_geometries[geometry.internal_id] = {};
        geometry.internal_id = numeric::invalid_id;
        geometry.generation = numeric::invalid_id;
    }

    bool VulkanRenderer::release_geometry_ranges(
        const VulkanGeometryData& geometry) noexcept {
        if (geometry.id == numeric::invalid_id ||
            geometry.vertex_range.size == 0) {
            return false;
        }

        if (geometry.index_range.size != 0) {
            auto index_released = m_object_index_buffer.release(
                geometry.index_range);
            if (!index_released)
                return false;
        }

        auto vertex_released = m_object_vertex_buffer.release(
            geometry.vertex_range);
        return static_cast<bool>(vertex_released);
    }

    result<void, renderer_error> VulkanRenderer::recreate_framebuffers() {
        if (m_device.dynamic_rendering())
            return ok();
        const u32 image_count = m_swapchain.get_image_count();

        if (image_count != m_world_framebuffers.length()) {
            if (!m_world_framebuffers.dyarr_resize(image_count) ||
                !m_ui_framebuffers.dyarr_resize(image_count))
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
        }

        for (u32 i = 0; i < m_world_framebuffers.length(); i++) {
            cl::arr<VkImageView> world_attachments;
            // clang-format off
            if (!world_attachments.arr_init_list(m_allocator, {
                m_swapchain.get_image_view_at(i),
                m_swapchain.get_depth_attachment(i)->get_view(),
            })) {
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
            }
            // clang-format on
            auto world_renewed = m_world_framebuffers[i].renew(
                m_framebuffer_width,
                m_framebuffer_height,
                world_attachments,
                &m_device,
                m_world_render_pass,
                m_vulkan_allocator);
            if (!world_renewed)
                return err(world_renewed.error());

            cl::arr<VkImageView> ui_attachments;
            if (!ui_attachments.arr_init_list(
                    m_allocator, {m_swapchain.get_image_view_at(i)})) {
                return err(renderer_error{
                    .code = renderer_error_code::out_of_memory,
                    .native_code = 0,
                });
            }
            auto ui_renewed = m_ui_framebuffers[i].renew(
                m_framebuffer_width,
                m_framebuffer_height,
                ui_attachments,
                &m_device,
                m_ui_render_pass,
                m_vulkan_allocator);
            if (!ui_renewed)
                return err(ui_renewed.error());
        }
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::recreate_command_buffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (m_timestamp_pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(m_device, m_timestamp_pool, m_vulkan_allocator);
            m_timestamp_pool = VK_NULL_HANDLE;
        }
        m_timestamp_pending.arr_shutdown();
        const cstr benchmark = std::getenv("NK_BENCHMARK");
        if (benchmark != nullptr && std::strcmp(benchmark, "1") == 0 &&
            m_device.timestamp_valid_bits() != 0) {
            if (!m_timestamp_pending.arr_init(m_allocator, image_count))
                return err(renderer_error{renderer_error_code::out_of_memory, 0});
            VkQueryPoolCreateInfo query_info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            query_info.queryCount = image_count * 2;
            const VkResult created = vkCreateQueryPool(
                m_device, &query_info, m_vulkan_allocator, &m_timestamp_pool);
            if (created != VK_SUCCESS)
                return err(renderer_error{renderer_error_code::initialization_failed,
                    static_cast<i32>(created)});
        }

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

        // Called only at startup or after draining for swapchain recreation.
        // Present-complete semaphores belong to images, acquire/fences to slots.
        for (VkSemaphore& semaphore : m_queue_complete_semaphores) {
            if (semaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(m_device, semaphore, m_vulkan_allocator);
            semaphore = VK_NULL_HANDLE;
        }
        for (VkSemaphore& semaphore : m_image_available_semaphores) {
            if (semaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(m_device, semaphore, m_vulkan_allocator);
            semaphore = VK_NULL_HANDLE;
        }
        if (!m_queue_complete_semaphores.dyarr_resize(image_count))
            return err(renderer_error{renderer_error_code::out_of_memory, 0});

        if (max_frames_in_flight != m_image_available_semaphores.length()) {
            if (!m_image_available_semaphores.dyarr_resize(max_frames_in_flight)) {
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
        }
        for (u32 i = 0; i < image_count; ++i) {
            VkSemaphoreCreateInfo semaphore_create_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
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

        // Framebuffers must release their references to the old swapchain
        // image views and depth attachment before those resources are replaced.
        for (Framebuffer& framebuffer : m_ui_framebuffers)
            framebuffer.shutdown();
        for (Framebuffer& framebuffer : m_world_framebuffers)
            framebuffer.shutdown();

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

        VkRect2D& world_render_area = m_world_render_pass.get_render_area();
        world_render_area.offset = {0, 0};
        world_render_area.extent = {m_framebuffer_width, m_framebuffer_height};
        VkRect2D& ui_render_area = m_ui_render_pass.get_render_area();
        ui_render_area.offset = {0, 0};
        ui_render_area.extent = {m_framebuffer_width, m_framebuffer_height};

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
            true,
            m_allocator,
            geometry_range_capacity);
        if (!vertex_buffer_initialized)
            return err(vertex_buffer_initialized.error());
        
        constexpr u64 index_buffer_size = sizeof(u32) * 1024 * 1024;
        auto index_buffer_initialized = m_object_index_buffer.init(
            &m_device,
            m_vulkan_allocator,
            index_buffer_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            memory_property_flags,
            true,
            m_allocator,
            geometry_range_capacity);
        if (!index_buffer_initialized)
            return err(index_buffer_initialized.error());

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
        const void* data
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
        auto staged = staging.load_data(0, size, 0, data);
        if (!staged)
            return err(staged.error());

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
