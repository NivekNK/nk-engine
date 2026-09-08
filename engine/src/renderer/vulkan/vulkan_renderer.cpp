#include "nkpch.h"

#include "vulkan/vulkan_renderer.h"

#include "platform/platform.h"
#include "systems/resource_system.h"
#include "vulkan/utils.h"
#include "vulkan/resources/texture_data.h"
#include "vulkan/graphics_commands.h"
#include "vulkan/image_utils.h"
#include "vulkan/texture_format.h"

#include <glm/vertex_3d.h>
#include <glm/vertex_2d.h>

namespace nk {
    namespace {
        VkAttachmentLoadOp attachment_load(
            const RenderLoadOperation operation) noexcept {
            switch (operation) {
                case RenderLoadOperation::clear:
                    return VK_ATTACHMENT_LOAD_OP_CLEAR;
                case RenderLoadOperation::load:
                    return VK_ATTACHMENT_LOAD_OP_LOAD;
                case RenderLoadOperation::discard:
                    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            }
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }

        VkAttachmentStoreOp attachment_store(
            const RenderStoreOperation operation) noexcept {
            return operation == RenderStoreOperation::store
                ? VK_ATTACHMENT_STORE_OP_STORE
                : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }

        VkImageUsageFlags texture_usage_flags(
            const TextureUsage usages) noexcept {
            VkImageUsageFlags result = 0;
            if (has_usage(usages, TextureUsage::sampled))
                result |= VK_IMAGE_USAGE_SAMPLED_BIT;
            if (has_usage(usages, TextureUsage::color_attachment))
                result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (has_usage(usages, TextureUsage::depth_stencil_attachment))
                result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            if (has_usage(usages, TextureUsage::transfer_source))
                result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            if (has_usage(usages, TextureUsage::transfer_destination))
                result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            return result;
        }

        vk::ImageUse resting_image_use(
            const TextureUsage usages,
            const bool depth) noexcept {
            if (depth)
                return vk::ImageUse::depth_attachment;
            return has_usage(usages, TextureUsage::sampled)
                ? vk::ImageUse::sampled
                : vk::ImageUse::color_attachment;
        }

        vk::ImageUse attachment_image_use(
            const RenderAttachmentUse use) noexcept {
            switch (use) {
                case RenderAttachmentUse::color_attachment:
                    return vk::ImageUse::color_attachment;
                case RenderAttachmentUse::depth_stencil_attachment:
                    return vk::ImageUse::depth_attachment;
                case RenderAttachmentUse::present:
                    return vk::ImageUse::present;
                case RenderAttachmentUse::sampled:
                    return vk::ImageUse::sampled;
                case RenderAttachmentUse::transfer_source:
                    return vk::ImageUse::transfer_source;
            }
            return vk::ImageUse::discard;
        }
    }

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
            m_default_texture, &m_samplers, m_default_sampler);
        if (!initialized) {
            (void)m_allocator->deconstruct_t(VulkanShader, shader);
            return err(initialized.error());
        }

        VulkanShaderSlot& slot = m_shaders[slot_index];
        slot.shader = shader;
        slot.render_pass = render_pass;
        slot.signature = compatible_render_pass->signature();
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
        slot.signature = {};
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
            slot.signature = {};
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
            m_shaders[handle.index].signature !=
                m_active_render_pass_signature ||
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

    result<void, renderer_error> VulkanRenderer::set_material_blend_mode(
        const MaterialBlendMode blend_mode) {
        VulkanShader* shader = resolve_shader(m_active_shader);
        if (shader == nullptr || !m_render_pass_active ||
            m_image_index >= m_graphics_command_buffers.length()) {
            return err(renderer_error{
                renderer_error_code::shader_state_invalid,
                0,
            });
        }
        return shader->use(
            m_graphics_command_buffers[m_image_index],
            blend_mode);
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
        TextureBinding binding,
        const u32 array_index) {
        VulkanShader* shader = resolve_shader(handle);
        if (shader == nullptr || handle != m_active_shader)
            return err(renderer_error{
                shader == nullptr
                    ? renderer_error_code::shader_handle_invalid
                    : renderer_error_code::shader_state_invalid,
                0,
            });
        return shader->set_sampler(uniform, binding, array_index);
    }

    bool VulkanRenderer::sampler_mutation_allowed() const noexcept {
        for (const CommandBuffer& commands : m_graphics_command_buffers)
            if (commands.state() == CommandBufferState::Recording ||
                commands.state() == CommandBufferState::InRenderPass ||
                commands.state() == CommandBufferState::RecordingEnded) return false;
        return true;
    }

    result<SamplerHandle, renderer_error> VulkanRenderer::create_sampler(const SamplerConfig& config) {
        if (!sampler_mutation_allowed())
            return err(renderer_error{renderer_error_code::shader_state_invalid, 0});
        return m_samplers.create(config);
    }

    result<void, renderer_error> VulkanRenderer::release_sampler(SamplerHandle sampler) {
        if (!sampler_mutation_allowed())
            return err(renderer_error{renderer_error_code::shader_state_invalid, 0});
        if (sampler == m_default_sampler)
            return err(renderer_error{renderer_error_code::sampler_handle_invalid, 0});
        return m_samplers.release(sampler);
    }

    void VulkanRenderer::on_default_texture_changed(Texture* texture) {
        for (VulkanShaderSlot& slot : m_shaders) {
            if (slot.shader != nullptr)
                slot.shader->set_default_texture(texture);
        }
        m_pick_world_shader.set_default_texture(texture);
        m_pick_ui_shader.set_default_texture(texture);
        m_pick_text_shader.set_default_texture(texture);
    }

    result<void, renderer_error> VulkanRenderer::init() {
        m_vulkan_allocator = nullptr;
        const cstr host_allocator_option =
            std::getenv("NK_VULKAN_HOST_ALLOCATOR");
        if (host_allocator_option != nullptr &&
            host_allocator_option[0] == '1' &&
            host_allocator_option[1] == '\0') {
            if (m_vulkan_host_memory.allocator_init(
                    mem::SynchronizedAllocator,
                    "Vulkan host allocations",
                    MemoryType::Renderer,
                    m_vulkan_host_backing) == nullptr ||
                !m_vulkan_host_allocator.init(m_vulkan_host_memory)) {
                return err(renderer_error{
                    renderer_error_code::initialization_failed,
                    0,
                });
            }
            m_vulkan_allocator = m_vulkan_host_allocator.callbacks();
            InfoLog("Vulkan host allocation callbacks enabled for diagnostics.");
        }

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
        auto samplers_initialized = m_samplers.init(*m_allocator, m_device, m_vulkan_allocator,
            {m_device.sampler_anisotropy(), m_device.max_sampler_anisotropy(), m_device.max_sampler_count()}, {});
        if (!samplers_initialized) return err(samplers_initialized.error());
        auto default_sampler = m_samplers.create({});
        if (!default_sampler) return err(default_sampler.error());
        m_default_sampler = *default_sampler;
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

        const TextureFormat color_format =
            vk::texture_format(m_swapchain.get_image_format().format);
        const TextureFormat depth_format =
            vk::texture_format(m_device.get_depth_format());
        if (color_format == TextureFormat::unknown ||
            depth_format == TextureFormat::unknown) {
            return err(renderer_error{
                renderer_error_code::render_target_config_invalid, 0});
        }
        auto pass_configs_initialized = m_window_render_passes.init(
            m_framebuffer_width,
            m_framebuffer_height,
            color_format,
            depth_format);
        if (!pass_configs_initialized) {
            return err(renderer_error{
                renderer_error_code::render_target_config_invalid,
                static_cast<i32>(pass_configs_initialized.error()),
            });
        }

        auto world_render_pass_initialized = m_world_render_pass.init(
            m_window_render_passes.world(), &m_device, m_vulkan_allocator
        );
        m_world_render_pass_initialized = true;
        if (!world_render_pass_initialized)
            return err(world_render_pass_initialized.error());

        auto ui_render_pass_initialized = m_ui_render_pass.init(
            m_window_render_passes.ui(), &m_device, m_vulkan_allocator
        );
        m_ui_render_pass_initialized = true;
        if (!ui_render_pass_initialized)
            return err(ui_render_pass_initialized.error());
        const u32 image_count = m_swapchain.get_image_count();
        auto targets_created = recreate_render_targets();
        if (!targets_created)
            return err(targets_created.error());
        InfoLog(
            "Vulkan world/UI render targets created ({} each, {} path).",
            m_world_targets.length(),
            m_device.dynamic_rendering() ? "dynamic" : "render-pass");

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

        auto picking_initialized = init_picking();
        if (!picking_initialized)
            return err(picking_initialized.error());

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
        m_timestamp_frames.arr_shutdown();

        shutdown_picking();

        m_geometry_upload_buffer.shutdown();
        if (m_text_frames.allocator()) (void)m_text_frames.arr_shutdown();
        m_object_vertex_buffer.shutdown();
        m_object_index_buffer.shutdown();
        if (m_geometry_upload_batches != 0) {
            InfoLog(
                "Geometry uploads: {} batch(es), {} copy command(s), {} byte(s).",
                m_geometry_upload_batches,
                m_geometry_upload_copies,
                m_geometry_upload_bytes);
        }
        m_geometry_upload_batches = 0;
        m_geometry_upload_copies = 0;
        m_geometry_upload_bytes = 0;
        InfoLog("Vulkan Object Buffers shutdown.");

        destroy_all_shaders();
        InfoLog("Vulkan sampler teardown: {} native sampler(s) remaining (renderer fallback included).", m_samplers.live_count());
        m_samplers.shutdown();
        m_default_sampler = {};
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
        m_ui_targets.dyarr_shutdown();
        m_world_targets.dyarr_shutdown();
        InfoLog("Vulkan world/UI render targets shutdown.");

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
        if (m_vulkan_allocator != nullptr) {
            const vk::VulkanHostAllocatorStatistics host =
                m_vulkan_host_allocator.statistics();
            InfoLog(
                "Vulkan host allocator peak: {} byte(s), {} active allocation(s).",
                host.peak_allocated_bytes,
                host.active_allocations);
            if (!m_vulkan_host_allocator.shutdown()) {
                ErrorLog(
                    "Vulkan host allocator shutdown found {} active allocation(s) and {} internal allocation(s).",
                    host.active_allocations,
                    host.internal_allocations);
            }
            m_vulkan_allocator = nullptr;
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
        auto pick_retired = retire_pick_readback();
        if (!pick_retired)
            return err(pick_retired.error());

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
        m_gpu_frame_number = 0;
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
                    m_gpu_frame_number = m_timestamp_frames[m_image_index];
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

        const vk::GraphicsCommands graphics{m_device, command_buffer};
        graphics.set_viewport(viewport);
        graphics.set_scissor(scissor);

        return ok(frame_outcome::rendered);
    }

    result<frame_outcome, renderer_error> VulkanRenderer::end_frame(f64) {
        CommandBuffer& command_buffer = m_graphics_command_buffers[m_image_index];

        if (m_timestamp_pool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                m_timestamp_pool, m_image_index * 2 + 1);
            m_timestamp_pending[m_image_index] = true;
            m_timestamp_frames[m_image_index] = m_frame_number;
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

        if (m_recorded_pick_slot >= 0) {
            PickReadback& readback =
                m_pick_readbacks[static_cast<u32>(m_recorded_pick_slot)];
            readback.pending = true;
            m_recorded_pick_slot = -1;
        }

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
            RenderTarget& target = world
                ? m_world_targets[m_image_index]
                : m_ui_targets[m_image_index];
            Assert(target.current(), "Render target attachments are stale.");
            Texture* color_texture =
                target.attachment(RenderAttachmentRole::color);
            Texture* depth_texture =
                target.attachment(RenderAttachmentRole::depth);
            TextureData* color_data = static_cast<TextureData*>(
                color_texture->m_internal_data);
            TextureData* depth_data = depth_texture == nullptr
                ? nullptr
                : static_cast<TextureData*>(depth_texture->m_internal_data);
            RenderPass& render_pass = world
                ? m_world_render_pass
                : m_ui_render_pass;
            const RenderPassConfig& pass_config = world
                ? m_window_render_passes.world()
                : m_window_render_passes.ui();
            const RenderAttachmentConfig* color_config =
                render_pass.attachment(RenderAttachmentRole::color);
            const RenderAttachmentConfig* depth_config =
                render_pass.attachment(RenderAttachmentRole::depth);

            if (!pass_config.has_previous_pass) {
                commands.transition(color_data->image.get(),
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                    vk::ImageUse::discard, vk::ImageUse::color_attachment);
                VkImageAspectFlags depth_aspects = VK_IMAGE_ASPECT_DEPTH_BIT;
                if (depth_texture != nullptr &&
                    depth_texture->format != TextureFormat::depth32_float) {
                    depth_aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
                if (depth_data != nullptr) {
                    commands.transition(depth_data->image.get(),
                        {depth_aspects, 0, 1, 0, 1},
                        vk::ImageUse::discard,
                        vk::ImageUse::depth_attachment);
                }
            } else {
                commands.barrier(
                    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT},
                    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT});
            }
            const glm::vec4& clear = render_pass.clear_color();
            commands.begin_rendering({
                .color = color_data->image.get_view(),
                .depth = depth_data == nullptr
                    ? VK_NULL_HANDLE
                    : depth_data->image.get_view(),
                .area = render_pass.get_render_area(),
                .clear_color = {{clear.r, clear.g, clear.b, clear.a}},
                .clear_depth = {
                    pass_config.clear_depth,
                    pass_config.clear_stencil,
                },
                .color_load = attachment_load(color_config->load),
                .color_store = attachment_store(color_config->store),
                .depth_load = depth_config == nullptr
                    ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                    : attachment_load(depth_config->load),
                .depth_store = depth_config == nullptr
                    ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                    : attachment_store(depth_config->store),
            });
            command_buffer.set_state(CommandBufferState::InRenderPass);
        } else {
            const bool target_is_current = pass == RenderPassKind::world
                ? m_world_targets[m_image_index].current()
                : m_ui_targets[m_image_index].current();
            Assert(
                target_is_current,
                "Render target attachments are stale.");
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
        m_active_render_pass_signature =
            (pass == RenderPassKind::world
                ? m_world_render_pass
                : m_ui_render_pass).signature();
        m_render_pass_active = true;
    }

    void VulkanRenderer::end_render_pass(const RenderPassKind pass) {
        CommandBuffer& command_buffer =
            m_graphics_command_buffers[m_image_index];
        if (m_device.dynamic_rendering()) {
            const vk::GraphicsCommands commands{m_device, command_buffer};
            commands.end_rendering();
            const bool world = pass == RenderPassKind::world;
            RenderTarget& target = world
                ? m_world_targets[m_image_index]
                : m_ui_targets[m_image_index];
            const Texture* color_texture =
                target.attachment(RenderAttachmentRole::color);
            const RenderAttachmentConfig* color_config = (world
                ? m_world_render_pass
                : m_ui_render_pass).attachment(RenderAttachmentRole::color);
            if (color_config != nullptr &&
                color_config->final_use !=
                    RenderAttachmentUse::color_attachment) {
                const TextureData* color_data = static_cast<const TextureData*>(
                    color_texture->m_internal_data);
                commands.transition(color_data->image.get(),
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                    vk::ImageUse::color_attachment,
                    attachment_image_use(color_config->final_use));
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
        m_active_render_pass_signature = {};
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
        if (data.geometry->material->type != expected_material_type &&
            !(pass == RenderPassKind::world &&
              data.geometry->material->type == MaterialType::skybox)) {
            ErrorLog("Geometry material type does not match the active render pass.");
            return;
        }

        const u64 expected_vertex_stride = pass == RenderPassKind::world
            ? sizeof(glm::Vertex3D)
            : sizeof(glm::Vertex2D);
        if (!m_object_vertex_buffer.valid(geometry.vertex_range) ||
            geometry.vertex_range.size !=
                geometry.vertex_count * expected_vertex_stride ||
            (geometry.index_count != 0 &&
             (!m_object_index_buffer.valid(geometry.index_range) ||
              geometry.index_range.size !=
                  geometry.index_count * sizeof(u32)))) {
            ErrorLog("Geometry buffer range or layout is invalid for the active render pass.");
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
        texture.format = unorm_texture_format(channel_count);
        texture.generation = 0;
        texture.state = TextureState::ready;

        TextureData* texture_data = m_allocator->construct_t(TextureData);
        if (texture_data == nullptr)
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        if (width > m_device.max_texture_dimension() || height > m_device.max_texture_dimension()) {
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(renderer_error{renderer_error_code::texture_limits_exceeded, 0});
        }
        texture.m_internal_data = texture_data;
        const VkDeviceSize image_size =
            static_cast<VkDeviceSize>(width) * height * channel_count;

        // NOTE: Assumes 8 bits per channel.
        const VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;
        const cstr mip_option = std::getenv("NK_VULKAN_MIPMAPS");
        const bool mipmaps = (mip_option == nullptr || std::strcmp(mip_option, "0") != 0) &&
            m_device.supports_linear_blit(image_format);
        const u32 mip_levels = mipmaps ? vk::mip_level_count({width, height}) : 1;

        // Create a staging buffer and load data into it.
        Buffer staging;
        auto staging_initialized = staging.init(
            &m_device,
            m_vulkan_allocator,
            {
                .size = image_size,
                .usage = BufferUsage::transfer_source,
                .memory = MemoryUsage::upload,
                .persistent_map = true,
            });
        if (!staging_initialized) {
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(staging_initialized.error());
        }

        auto staged = staging.upload(0, image_size, pixels);
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
                .usage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                    (mip_levels > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0)),
                .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .create_view = true,
                .view_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
                .mip_levels = mip_levels,
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

        const vk::GraphicsCommands commands{m_device, temp_buffer};
        commands.transition(texture_data->image.get(), {VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, 1},
            vk::ImageUse::discard, vk::ImageUse::transfer_destination);

        // Copy the data from the buffer.
        texture_data->image.copy_from_buffer(&temp_buffer, staging);

        // One upload submission, never per-frame work. The single-level
        // capability/diagnostic fallback still transitions to sampled use.
        texture_data->image.generate_mipmaps(temp_buffer);

        auto command_ended = temp_buffer.end_single_use(queue);
        if (!command_ended) {
            texture_data->image.shutdown();
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(command_ended.error());
        }
        
        texture.flags = has_transparency
            ? TextureFlag::has_transparency
            : TextureFlag::none;
        texture.usage = TextureUsage::sampled |
            TextureUsage::transfer_destination |
            (mip_levels > 1
                ? TextureUsage::transfer_source
                : TextureUsage::none);
        DebugLog("Texture '{}' uploaded with {} mip level(s).", name, mip_levels);
        *out_texture = texture;
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::create_texture_cube(
        const strview name,
        const u32 width,
        const u32 height,
        const u32 channel_count,
        const u8* face_pixels,
        Texture* out_texture) {
        constexpr u32 face_count = 6;
        if (out_texture == nullptr || face_pixels == nullptr || width == 0 ||
            height == 0 || channel_count != 4) {
            return err(renderer_error{
                renderer_error_code::texture_state_invalid,
                0,
            });
        }
        if (width > m_device.max_texture_dimension() ||
            height > m_device.max_texture_dimension()) {
            return err(renderer_error{
                renderer_error_code::texture_limits_exceeded,
                0,
            });
        }

        const VkDeviceSize face_size =
            static_cast<VkDeviceSize>(width) * height * channel_count;
        if (face_size > numeric::u64_max / face_count) {
            return err(renderer_error{
                renderer_error_code::texture_limits_exceeded,
                0,
            });
        }
        const VkDeviceSize image_size = face_size * face_count;
        constexpr VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;
        const cstr mip_option = std::getenv("NK_VULKAN_MIPMAPS");
        const bool mipmaps =
            (mip_option == nullptr || std::strcmp(mip_option, "0") != 0) &&
            m_device.supports_linear_blit(image_format);
        const u32 mip_levels = mipmaps
            ? vk::mip_level_count({width, height})
            : 1;

        TextureData* texture_data = m_allocator->construct_t(TextureData);
        if (texture_data == nullptr)
            return err(renderer_error{renderer_error_code::out_of_memory, 0});

        Buffer staging;
        auto staging_initialized = staging.init(
            &m_device,
            m_vulkan_allocator,
            {
                .size = image_size,
                .usage = BufferUsage::transfer_source,
                .memory = MemoryUsage::upload,
                .persistent_map = true,
            });
        if (!staging_initialized) {
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(staging_initialized.error());
        }
        auto staged = staging.upload(0, image_size, face_pixels);
        if (!staged) {
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(staged.error());
        }

        auto image_initialized = texture_data->image.init(
            {
                .image_type = VK_IMAGE_TYPE_2D,
                .extent = {width, height},
                .format = image_format,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = static_cast<VkImageUsageFlags>(
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT |
                    (mip_levels > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0)),
                .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .create_view = true,
                .view_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
                .mip_levels = mip_levels,
                .layer_count = face_count,
                .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .view_type = VK_IMAGE_VIEW_TYPE_CUBE,
            },
            &m_device,
            m_vulkan_allocator);
        if (!image_initialized) {
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(image_initialized.error());
        }

        CommandBuffer commands;
        auto begun = commands.init(
            m_device.get_graphics_command_pool(),
            &m_device,
            true,
            true);
        if (!begun) {
            const renderer_error error = begun.error();
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(error);
        }
        const vk::GraphicsCommands graphics{m_device, commands};
        graphics.transition(
            texture_data->image.get(),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, mip_levels, 0, face_count},
            vk::ImageUse::discard,
            vk::ImageUse::transfer_destination);
        texture_data->image.copy_layers_from_buffer(
            &commands,
            staging,
            face_count,
            face_size);
        texture_data->image.generate_mipmaps(commands);
        auto completed = commands.end_single_use(
            m_device.get_graphics_queue());
        if (!completed) {
            const renderer_error error = completed.error();
            m_allocator->deconstruct_t(TextureData, texture_data);
            return err(error);
        }

        *out_texture = {
            .width = width,
            .height = height,
            .channel_count = static_cast<u8>(channel_count),
            .dimension = TextureDimension::cube,
            .layer_count = face_count,
            .format = TextureFormat::rgba8_unorm,
            .usage = TextureUsage::sampled |
                TextureUsage::transfer_destination |
                TextureUsage::transfer_source,
            .generation = 0,
            .state = TextureState::ready,
            .m_internal_data = texture_data,
        };
        DebugLog(
            "Cube texture '{}' uploaded with {} mip level(s).",
            name,
            mip_levels);
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::create_writable_texture(
        Texture* texture) {
        if (!m_device_initialized || m_allocator == nullptr ||
            texture == nullptr || texture->m_internal_data != nullptr ||
            texture->width == 0 || texture->height == 0 ||
            !texture->writable() || texture->external() ||
            texture->dimension != TextureDimension::texture_2d ||
            texture->layer_count != 1) {
            return err(renderer_error{
                renderer_error_code::texture_state_invalid,
                0,
            });
        }
        const TextureFormat requested_format =
            texture->format == TextureFormat::unknown
                ? unorm_texture_format(texture->channel_count)
                : texture->format;
        const VkFormat format = vk::texture_format(requested_format);
        if (format == VK_FORMAT_UNDEFINED)
            return err(renderer_error{
                renderer_error_code::texture_state_invalid,
                0,
            });
        if (texture->width > m_device.max_texture_dimension() ||
            texture->height > m_device.max_texture_dimension()) {
            return err(renderer_error{
                renderer_error_code::texture_limits_exceeded,
                0,
            });
        }

        TextureUsage usages = texture->usage;
        if (usages == TextureUsage::none) {
            usages = TextureUsage::sampled |
                TextureUsage::color_attachment |
                TextureUsage::transfer_destination;
        }
        const VkImageUsageFlags image_usage = texture_usage_flags(usages);
        const bool depth = is_depth_format(requested_format);
        if (image_usage == 0 ||
            (depth && has_usage(usages, TextureUsage::color_attachment)) ||
            (!depth && has_usage(
                usages, TextureUsage::depth_stencil_attachment))) {
            return err(renderer_error{
                renderer_error_code::texture_state_invalid,
                0,
            });
        }

        TextureData* data = m_allocator->construct_t(TextureData);
        if (data == nullptr)
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        auto initialized = data->image.init(
            {
                .image_type = VK_IMAGE_TYPE_2D,
                .extent = {texture->width, texture->height},
                .format = format,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = image_usage,
                .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .create_view = true,
                .view_aspect_flags = depth
                    ? VK_IMAGE_ASPECT_DEPTH_BIT
                    : VK_IMAGE_ASPECT_COLOR_BIT,
                .mip_levels = 1,
            },
            &m_device,
            m_vulkan_allocator);
        if (!initialized) {
            const renderer_error error = initialized.error();
            m_allocator->deconstruct_t(TextureData, data);
            return err(error);
        }

        CommandBuffer commands;
        auto begun = commands.init(
            m_device.get_graphics_command_pool(),
            &m_device,
            true,
            true);
        if (!begun) {
            const renderer_error error = begun.error();
            m_allocator->deconstruct_t(TextureData, data);
            return err(error);
        }
        const vk::GraphicsCommands graphics{m_device, commands};
        graphics.transition(
            data->image.get(),
            {depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                0, 1, 0, 1},
            vk::ImageUse::discard,
            resting_image_use(usages, depth));
        auto completed = commands.end_single_use(m_device.get_graphics_queue());
        if (!completed) {
            const renderer_error error = completed.error();
            m_allocator->deconstruct_t(TextureData, data);
            return err(error);
        }

        texture->format = requested_format;
        texture->usage = usages;
        texture->m_internal_data = data;
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::write_texture(
        Texture& texture,
        const TextureRegion region,
        const cl::slice<const u8> pixels) {
        if (!m_device_initialized || m_allocator == nullptr ||
            !texture.valid() || !texture.writable() || texture.external() ||
            !has_usage(texture.usage, TextureUsage::transfer_destination) ||
            vk::texture_format(texture.format) == VK_FORMAT_UNDEFINED ||
            pixels.data() == nullptr || region.width == 0 ||
            region.height == 0 || region.x >= texture.width ||
            region.y >= texture.height ||
            region.width > texture.width - region.x ||
            region.height > texture.height - region.y) {
            return err(renderer_error{
                renderer_error_code::texture_region_invalid,
                0,
            });
        }
        const u64 size = static_cast<u64>(region.width) * region.height *
            texture_format_texel_size(texture.format);
        if (pixels.length() != size)
            return err(renderer_error{
                renderer_error_code::texture_region_invalid,
                0,
            });

        Buffer staging;
        auto staging_initialized = staging.init(
            &m_device,
            m_vulkan_allocator,
            {
                .size = size,
                .usage = BufferUsage::transfer_source,
                .memory = MemoryUsage::upload,
                .persistent_map = true,
            });
        if (!staging_initialized)
            return err(staging_initialized.error());
        auto staged = staging.upload(0, size, pixels.data());
        if (!staged)
            return err(staged.error());

        CommandBuffer commands;
        auto begun = commands.init(
            m_device.get_graphics_command_pool(),
            &m_device,
            true,
            true);
        if (!begun)
            return err(begun.error());

        TextureData* data = static_cast<TextureData*>(texture.m_internal_data);
        const vk::GraphicsCommands graphics{m_device, commands};
        const VkImageSubresourceRange whole{
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            1,
            0,
            1,
        };
        graphics.transition(
            data->image.get(),
            whole,
            resting_image_use(
                texture.usage,
                is_depth_format(texture.format)),
            vk::ImageUse::transfer_destination);
        data->image.copy_from_buffer(
            &commands,
            staging,
            region.x,
            region.y,
            region.width,
            region.height);
        graphics.transition(
            data->image.get(),
            whole,
            vk::ImageUse::transfer_destination,
            resting_image_use(
                texture.usage,
                is_depth_format(texture.format)));
        return commands.end_single_use(m_device.get_graphics_queue());
    }

    result<void, renderer_error> VulkanRenderer::resize_texture(
        Texture& texture,
        const u32 width,
        const u32 height) {
        if (!m_device_initialized || m_allocator == nullptr ||
            !texture.valid() || !texture.writable() || texture.external() ||
            width == 0 || height == 0) {
            return err(renderer_error{
                renderer_error_code::texture_state_invalid,
                0,
            });
        }
        if (width > m_device.max_texture_dimension() ||
            height > m_device.max_texture_dimension()) {
            return err(renderer_error{
                renderer_error_code::texture_limits_exceeded,
                0,
            });
        }
        const VkFormat format = vk::texture_format(texture.format);
        if (format == VK_FORMAT_UNDEFINED)
            return err(renderer_error{
                renderer_error_code::texture_state_invalid,
                0,
            });

        const bool depth = is_depth_format(texture.format);
        const VkImageUsageFlags image_usage =
            texture_usage_flags(texture.usage);
        if (image_usage == 0)
            return err(renderer_error{
                renderer_error_code::texture_state_invalid,
                0,
            });

        TextureData* replacement = m_allocator->construct_t(TextureData);
        if (replacement == nullptr)
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        auto initialized = replacement->image.init(
            {
                .image_type = VK_IMAGE_TYPE_2D,
                .extent = {width, height},
                .format = format,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = image_usage,
                .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .create_view = true,
                .view_aspect_flags = depth
                    ? VK_IMAGE_ASPECT_DEPTH_BIT
                    : VK_IMAGE_ASPECT_COLOR_BIT,
                .mip_levels = 1,
            },
            &m_device,
            m_vulkan_allocator);
        if (!initialized) {
            const renderer_error error = initialized.error();
            m_allocator->deconstruct_t(TextureData, replacement);
            return err(error);
        }

        CommandBuffer commands;
        auto begun = commands.init(
            m_device.get_graphics_command_pool(),
            &m_device,
            true,
            true);
        if (!begun) {
            const renderer_error error = begun.error();
            m_allocator->deconstruct_t(TextureData, replacement);
            return err(error);
        }
        const vk::GraphicsCommands graphics{m_device, commands};
        graphics.transition(
            replacement->image.get(),
            {depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                0, 1, 0, 1},
            vk::ImageUse::discard,
            resting_image_use(texture.usage, depth));
        auto completed = commands.end_single_use(m_device.get_graphics_queue());
        if (!completed) {
            const renderer_error error = completed.error();
            m_allocator->deconstruct_t(TextureData, replacement);
            return err(error);
        }

        TextureData* previous =
            static_cast<TextureData*>(texture.m_internal_data);
        texture.m_internal_data = replacement;
        m_allocator->deconstruct_t(TextureData, previous);
        return ok();
    }

    void VulkanRenderer::destroy_texture(Texture* texture) {
        if (texture == nullptr)
            std::abort();
        if (texture->external()) {
            WarnLog("External textures must be released by their owner.");
            return;
        }
        if (texture->m_internal_data == nullptr) {
            *texture = {};
            return;
        }
        auto retired = wait_for_in_flight_frames();
        if (!retired) {
            ErrorLog("Texture destruction could not retire in-flight frames.");
            return;
        }

        TextureData* texture_data = static_cast<TextureData*>(texture->m_internal_data);

        texture_data->image.shutdown();

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

        RenderBufferView index_range{};
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

        auto uploaded_ranges = upload_geometry_ranges(
            *vertex_reserved,
            vertices,
            index_range,
            indices.data());
        if (!uploaded_ranges) {
            bool rollback_succeeded = true;
            if (index_range.size != 0) {
                auto index_released = m_object_index_buffer.release(index_range);
                rollback_succeeded = static_cast<bool>(index_released);
            }
            auto vertex_released = m_object_vertex_buffer.release(
                *vertex_reserved);
            if (!rollback_succeeded || !vertex_released)
                ErrorLog("Failed to roll back geometry buffer ranges.");
            return err(uploaded_ranges.error());
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

    result<void, renderer_error> VulkanRenderer::upload_geometry_ranges(
        const RenderBufferView& vertices,
        const void* vertex_data,
        const RenderBufferView& indices,
        const void* index_data) {
        if (!m_object_vertex_buffer.valid(vertices) || vertex_data == nullptr ||
            (vertices.offset & 3) != 0 || (vertices.size & 3) != 0 ||
            (indices.size != 0 &&
             (!m_object_index_buffer.valid(indices) || index_data == nullptr ||
              (indices.offset & 3) != 0 || (indices.size & 3) != 0)) ||
            vertices.size > numeric::u64_max - 3) {
            return err(renderer_error{
                renderer_error_code::buffer_range_invalid,
                0,
            });
        }

        const u64 index_source_offset = (vertices.size + 3) & ~u64{3};
        if (indices.size > numeric::u64_max - index_source_offset) {
            return err(renderer_error{
                renderer_error_code::buffer_range_invalid,
                0,
            });
        }
        const u64 staging_size = index_source_offset + indices.size;
        if (!m_geometry_upload_buffer.initialized()) {
            return err(renderer_error{
                renderer_error_code::initialization_failed,
                0,
            });
        }
        if (staging_size > m_geometry_upload_buffer.size()) {
            u64 capacity = m_geometry_upload_buffer.size();
            while (capacity < staging_size) {
                if (capacity > numeric::u64_max / 2) {
                    capacity = staging_size;
                    break;
                }
                capacity *= 2;
            }
            auto resized = m_geometry_upload_buffer.resize(capacity);
            if (!resized)
                return err(resized.error());
        }
        auto vertices_staged = m_geometry_upload_buffer.upload(
            0, vertices.size, vertex_data);
        if (!vertices_staged)
            return err(vertices_staged.error());
        if (indices.size != 0) {
            auto indices_staged = m_geometry_upload_buffer.upload(
                index_source_offset, indices.size, index_data);
            if (!indices_staged)
                return err(indices_staged.error());
        }

        CommandBuffer commands;
        auto begun = commands.init(
            m_device.get_graphics_command_pool(),
            &m_device,
            true,
            true);
        if (!begun)
            return err(begun.error());

        const VkBufferCopy vertex_copy{
            .srcOffset = 0,
            .dstOffset = vertices.offset,
            .size = vertices.size,
        };
        vkCmdCopyBuffer(commands, m_geometry_upload_buffer.get(),
            m_object_vertex_buffer.get(), 1, &vertex_copy);

        const vk::GraphicsCommands graphics{m_device, commands};
        const vk::AccessScope transferred{
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
        };
        if (indices.size != 0) {
            const VkBufferCopy index_copy{
                .srcOffset = index_source_offset,
                .dstOffset = indices.offset,
                .size = indices.size,
            };
            vkCmdCopyBuffer(commands, m_geometry_upload_buffer.get(),
                m_object_index_buffer.get(), 1, &index_copy);
            graphics.buffer_barrier(
                {m_object_index_buffer.get(), indices.offset, indices.size},
                transferred,
                {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                    VK_ACCESS_INDEX_READ_BIT});
        }
        graphics.buffer_barrier(
            {m_object_vertex_buffer.get(), vertices.offset, vertices.size},
            transferred,
            {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT});
        auto completed = commands.end_single_use(
            m_device.get_graphics_queue());
        if (!completed)
            return err(completed.error());
        ++m_geometry_upload_batches;
        m_geometry_upload_copies += indices.size == 0 ? 1 : 2;
        m_geometry_upload_bytes += vertices.size + indices.size;
        return ok();
    }

    void VulkanRenderer::destroy_geometry(Geometry& geometry) {
        if (geometry.internal_id >= max_geometry_count)
            return;

        VulkanGeometryData& internal = m_geometries[geometry.internal_id];
        if (internal.id == numeric::invalid_id)
            return;

        auto retired = wait_for_in_flight_frames();
        if (!retired) {
            ErrorLog("Geometry destruction could not retire in-flight frames.");
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

    result<void, renderer_error>
    VulkanRenderer::wait_for_in_flight_frames() noexcept {
        for (Fence& fence : m_in_flight_fences) {
            auto retired = fence.wait(numeric::u64_max);
            if (!retired)
                return err(retired.error());
        }
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::recreate_render_targets() {
        const u32 image_count = m_swapchain.get_image_count();

        cl::dyarr<RenderTarget> world_targets;
        cl::dyarr<RenderTarget> ui_targets;
        if (!world_targets.dyarr_init_len(
                m_allocator, image_count, image_count) ||
            !ui_targets.dyarr_init_len(
                m_allocator, image_count, image_count)) {
            return err(renderer_error{
                renderer_error_code::out_of_memory, 0});
        }

        for (u32 index = 0; index < image_count; ++index) {
            Texture* world_attachments[]{
                m_swapchain.get_render_texture_at(index),
                m_swapchain.get_depth_texture_at(index),
            };
            auto world_created = world_targets[index].init(
                m_window_render_passes.world(),
                {
                    m_framebuffer_width,
                    m_framebuffer_height,
                    {world_attachments},
                });
            if (!world_created)
                return err(renderer_error{
                    renderer_error_code::render_target_config_invalid,
                    static_cast<i32>(world_created.error()),
                });

            Texture* ui_attachments[]{
                m_swapchain.get_render_texture_at(index),
            };
            auto ui_created = ui_targets[index].init(
                m_window_render_passes.ui(),
                {
                    m_framebuffer_width,
                    m_framebuffer_height,
                    {ui_attachments},
                });
            if (!ui_created)
                return err(renderer_error{
                    renderer_error_code::render_target_config_invalid,
                    static_cast<i32>(ui_created.error()),
                });
        }

        cl::dyarr<Framebuffer> world_framebuffers;
        cl::dyarr<Framebuffer> ui_framebuffers;
        if (!m_device.dynamic_rendering()) {
            if (!world_framebuffers.dyarr_init_len(
                    m_allocator, image_count, image_count) ||
                !ui_framebuffers.dyarr_init_len(
                    m_allocator, image_count, image_count)) {
                return err(renderer_error{
                    renderer_error_code::out_of_memory, 0});
            }
            for (u32 index = 0; index < image_count; ++index) {
                auto world_created = world_framebuffers[index].init(
                    world_targets[index],
                    &m_device,
                    m_world_render_pass,
                    m_vulkan_allocator);
                if (!world_created)
                    return err(world_created.error());
                auto ui_created = ui_framebuffers[index].init(
                    ui_targets[index],
                    &m_device,
                    m_ui_render_pass,
                    m_vulkan_allocator);
                if (!ui_created)
                    return err(ui_created.error());
            }
        }

        m_world_framebuffers = std::move(world_framebuffers);
        m_ui_framebuffers = std::move(ui_framebuffers);
        m_world_targets = std::move(world_targets);
        m_ui_targets = std::move(ui_targets);
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::recreate_command_buffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (m_timestamp_pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(m_device, m_timestamp_pool, m_vulkan_allocator);
            m_timestamp_pool = VK_NULL_HANDLE;
        }
        m_timestamp_pending.arr_shutdown();
        m_timestamp_frames.arr_shutdown();
        const cstr timestamp_option = std::getenv("NK_GPU_TIMESTAMPS");
        if ((timestamp_option == nullptr || std::strcmp(timestamp_option, "0") != 0) &&
            m_device.timestamp_valid_bits() != 0) {
            if (!m_timestamp_pending.arr_init(m_allocator, image_count) ||
                !m_timestamp_frames.arr_init(m_allocator, image_count))
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
        m_ui_targets.dyarr_shutdown();
        m_world_targets.dyarr_shutdown();

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
        auto pass_configs_resized = m_window_render_passes.resize(
            m_framebuffer_width,
            m_framebuffer_height);
        if (!pass_configs_resized) {
            return err(renderer_error{
                renderer_error_code::render_target_config_invalid,
                static_cast<i32>(pass_configs_resized.error()),
            });
        }

        if (m_picking_enabled) {
            m_pick_pass_config.area.width = m_framebuffer_width;
            m_pick_pass_config.area.height = m_framebuffer_height;
            VkRect2D& pick_area = m_pick_render_pass.get_render_area();
            pick_area.offset = {0, 0};
            pick_area.extent = {
                m_framebuffer_width,
                m_framebuffer_height,
            };
            auto pick_target_created = recreate_pick_target();
            if (!pick_target_created)
                return err(pick_target_created.error());
            for (PickReadback& readback : m_pick_readbacks) {
                readback.request = {};
                readback.submitted_frame = 0;
                readback.pending = false;
            }
        }

        auto targets_created = recreate_render_targets();
        if (!targets_created)
            return err(targets_created.error());
        auto commands_created = recreate_command_buffers();
        if (!commands_created)
            return err(commands_created.error());
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::create_buffers() {
        constexpr u64 upload_buffer_size = 8 * 1024 * 1024;
        auto upload_buffer_initialized = m_geometry_upload_buffer.init(
            &m_device,
            m_vulkan_allocator,
            {
                .size = upload_buffer_size,
                .usage = BufferUsage::transfer_source |
                    BufferUsage::transfer_destination,
                .memory = MemoryUsage::upload,
                .persistent_map = true,
            });
        if (!upload_buffer_initialized)
            return err(upload_buffer_initialized.error());

        constexpr u64 vertex_buffer_size = sizeof(glm::Vertex3D) * 1024 * 1024;
        auto vertex_buffer_initialized = m_object_vertex_buffer.init(
            &m_device,
            m_vulkan_allocator,
            {
                .size = vertex_buffer_size,
                .usage = BufferUsage::vertex |
                    BufferUsage::transfer_destination |
                    BufferUsage::transfer_source,
                .memory = MemoryUsage::device_local,
                .suballocation_capacity = geometry_range_capacity,
            },
            m_allocator);
        if (!vertex_buffer_initialized)
            return err(vertex_buffer_initialized.error());
        
        constexpr u64 index_buffer_size = sizeof(u32) * 1024 * 1024;
        auto index_buffer_initialized = m_object_index_buffer.init(
            &m_device,
            m_vulkan_allocator,
            {
                .size = index_buffer_size,
                .usage = BufferUsage::index |
                    BufferUsage::transfer_destination |
                    BufferUsage::transfer_source,
                .memory = MemoryUsage::device_local,
                .suballocation_capacity = geometry_range_capacity,
            },
            m_allocator);
        if (!index_buffer_initialized)
            return err(index_buffer_initialized.error());

        InfoLog("Vulkan Object Buffers created.");
        return ok();
    }

}
