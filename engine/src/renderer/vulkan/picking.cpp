#include "nkpch.h"

#include "vulkan/vulkan_renderer.h"

#include "renderer/text_renderer.h"
#include "resources/material.h"
#include "resources/shader_resource.h"
#include "systems/resource_system.h"
#include "vulkan/graphics_commands.h"
#include "vulkan/resources/texture_data.h"
#include "vulkan/texture_format.h"

#include <glm/gtc/matrix_transform.hpp>

namespace nk {
    namespace {
        constexpr ShaderUniformHandle pick_projection{0};
        constexpr ShaderUniformHandle pick_view{1};
        constexpr ShaderUniformHandle pick_diffuse_color{2};
        constexpr ShaderUniformHandle pick_diffuse_texture{3};
        constexpr ShaderUniformHandle pick_blend_mode{4};
        constexpr ShaderUniformHandle pick_alpha_cutoff{5};
        constexpr ShaderUniformHandle pick_model{6};
        constexpr ShaderUniformHandle pick_identity{7};

        constexpr ShaderUniformHandle pick_text_projection{0};
        constexpr ShaderUniformHandle pick_text_atlas{1};
        constexpr ShaderUniformHandle pick_text_identity{2};

        renderer_error resource_failure(const resource_error error) noexcept {
            return {
                renderer_error_code::initialization_failed,
                error.native_code,
            };
        }
    }

    result<void, renderer_error> VulkanRenderer::init_pick_shader(
        VulkanShader& shader,
        const strview resource_name) {
        auto resource = m_resources->load(resource_name, ResourceType::shader);
        if (!resource)
            return err(resource_failure(resource.error()));
        const ShaderResourceConfig* loaded =
            resource->as<ShaderResourceConfig>();
        if (loaded == nullptr) {
            (void)m_resources->unload(*resource);
            return err(renderer_error{
                renderer_error_code::shader_config_invalid, 0});
        }

        auto initialized = shader.init(
            loaded->config(),
            m_framebuffer_width,
            m_framebuffer_height,
            m_swapchain.get_max_frames_in_flight(),
            &m_pick_render_pass,
            &m_device,
            m_allocator,
            m_resources,
            m_vulkan_allocator,
            m_default_texture,
            &m_samplers,
            m_default_sampler);
        auto unloaded = m_resources->unload(*resource);
        if (!initialized)
            return err(initialized.error());
        if (!unloaded) {
            shader.shutdown();
            return err(resource_failure(unloaded.error()));
        }
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::init_picking() {
        const cstr option = std::getenv("NK_PICKING");
        if (option != nullptr && option[0] == '0' && option[1] == '\0') {
            InfoLog("GPU picking disabled by NK_PICKING=0.");
            return ok();
        }
        if (!m_device.supports_optimal_format(
                VK_FORMAT_R32_UINT,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                    VK_FORMAT_FEATURE_TRANSFER_SRC_BIT)) {
            return err(renderer_error{
                renderer_error_code::render_target_config_invalid,
                static_cast<i32>(VK_FORMAT_R32_UINT),
            });
        }

        m_pick_attachments[0] = {
            .role = RenderAttachmentRole::color,
            .source = RenderAttachmentSource::texture,
            .format = TextureFormat::r32_uint,
            .load = RenderLoadOperation::clear,
            .store = RenderStoreOperation::store,
            .final_use = RenderAttachmentUse::transfer_source,
            .resize = RenderAttachmentResize::window,
        };
        m_pick_attachments[1] = {
            .role = RenderAttachmentRole::depth,
            .source = RenderAttachmentSource::texture,
            .format = vk::texture_format(m_device.get_depth_format()),
            .load = RenderLoadOperation::clear,
            .store = RenderStoreOperation::discard,
            .final_use = RenderAttachmentUse::depth_stencil_attachment,
            .resize = RenderAttachmentResize::window,
        };
        m_pick_pass_config = {
            .name = "pick",
            .kind = RenderPassKind::world,
            .area = {0, 0, m_framebuffer_width, m_framebuffer_height},
            .clear_color = glm::vec4{0.0f},
            .clear_depth = 1.0f,
            .attachments = {m_pick_attachments},
        };
        auto pass_initialized = m_pick_render_pass.init(
            m_pick_pass_config, &m_device, m_vulkan_allocator);
        if (!pass_initialized)
            return err(pass_initialized.error());
        m_pick_render_pass_initialized = true;

        auto target_created = recreate_pick_target();
        if (!target_created)
            return err(target_created.error());

        const u32 frames = m_swapchain.get_max_frames_in_flight();
        if (!m_pick_readbacks.arr_init(m_allocator, frames))
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        for (PickReadback& readback : m_pick_readbacks) {
            auto initialized = readback.buffer.init(
                &m_device,
                m_vulkan_allocator,
                {
                    .size = sizeof(u32),
                    .usage = BufferUsage::transfer_destination,
                    .memory = MemoryUsage::readback,
                    .persistent_map = true,
                });
            if (!initialized)
                return err(initialized.error());
        }

        for (u32& instance : m_pick_world_material_instances)
            instance = numeric::invalid_id;
        for (u32& instance : m_pick_ui_material_instances)
            instance = numeric::invalid_id;
        for (u32& instance : m_pick_text_instances)
            instance = numeric::invalid_id;

        auto world_initialized = init_pick_shader(
            m_pick_world_shader, "Builtin.PickWorldShader");
        if (!world_initialized)
            return err(world_initialized.error());
        auto ui_initialized = init_pick_shader(
            m_pick_ui_shader, "Builtin.PickUIShader");
        if (!ui_initialized)
            return err(ui_initialized.error());
        auto text_initialized = init_pick_shader(
            m_pick_text_shader, "Builtin.PickTextShader");
        if (!text_initialized)
            return err(text_initialized.error());

        m_picking_enabled = true;
        InfoLog(
            "Asynchronous R32_UINT picking initialized ({} readback slots).",
            frames);
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::recreate_pick_target() {
        m_pick_framebuffer.shutdown();
        m_pick_target.reset();
        if (m_pick_color.m_internal_data != nullptr)
            destroy_texture(&m_pick_color);
        if (m_pick_depth.m_internal_data != nullptr)
            destroy_texture(&m_pick_depth);

        m_pick_color = {
            .width = m_framebuffer_width,
            .height = m_framebuffer_height,
            .channel_count = 1,
            .format = TextureFormat::r32_uint,
            .flags = TextureFlag::writable,
            .usage = TextureUsage::color_attachment |
                TextureUsage::transfer_source,
            .generation = 0,
            .state = TextureState::ready,
        };
        auto color_created = create_writable_texture(&m_pick_color);
        if (!color_created)
            return err(color_created.error());

        m_pick_depth = {
            .width = m_framebuffer_width,
            .height = m_framebuffer_height,
            .channel_count = 1,
            .format = vk::texture_format(m_device.get_depth_format()),
            .flags = TextureFlag::writable,
            .usage = TextureUsage::depth_stencil_attachment,
            .generation = 0,
            .state = TextureState::ready,
        };
        auto depth_created = create_writable_texture(&m_pick_depth);
        if (!depth_created)
            return err(depth_created.error());

        Texture* attachments[]{&m_pick_color, &m_pick_depth};
        auto target_created = m_pick_target.init(
            m_pick_pass_config,
            {
                m_framebuffer_width,
                m_framebuffer_height,
                {attachments},
            });
        if (!target_created) {
            return err(renderer_error{
                renderer_error_code::render_target_config_invalid,
                static_cast<i32>(target_created.error()),
            });
        }
        if (!m_device.dynamic_rendering()) {
            auto framebuffer_created = m_pick_framebuffer.init(
                m_pick_target,
                &m_device,
                m_pick_render_pass,
                m_vulkan_allocator);
            if (!framebuffer_created)
                return err(framebuffer_created.error());
        }
        m_pick_color_in_transfer = false;
        return ok();
    }

    void VulkanRenderer::shutdown_picking() noexcept {
        m_picking_enabled = false;
        m_recorded_pick_slot = -1;
        m_pick_text_shader.shutdown();
        m_pick_ui_shader.shutdown();
        m_pick_world_shader.shutdown();
        if (m_pick_readbacks.allocator() != nullptr)
            (void)m_pick_readbacks.arr_shutdown();
        m_pick_framebuffer.shutdown();
        m_pick_target.reset();
        if (m_pick_depth.m_internal_data != nullptr)
            destroy_texture(&m_pick_depth);
        if (m_pick_color.m_internal_data != nullptr)
            destroy_texture(&m_pick_color);
        if (m_pick_render_pass_initialized) {
            m_pick_render_pass.shutdown();
            m_pick_render_pass_initialized = false;
        }
        m_pick_pass_config = {};
        m_pick_color_in_transfer = false;
    }

    result<void, renderer_error> VulkanRenderer::retire_pick_readback() {
        if (!m_picking_enabled ||
            m_current_frame >= m_pick_readbacks.length()) {
            return ok();
        }
        PickReadback& readback = m_pick_readbacks[m_current_frame];
        if (!readback.pending)
            return ok();

        auto range = readback.buffer.view(0, sizeof(u32), alignof(u32));
        if (!range)
            return err(range.error());
        auto mapped = readback.buffer.map(*range);
        if (!mapped)
            return err(mapped.error());
        auto invalidated = mapped->invalidate();
        if (!invalidated)
            return err(invalidated.error());
        u32 encoded = 0;
        std::memcpy(&encoded, mapped->data(), sizeof(encoded));
        publish_pick_sample(
            readback.request,
            PickId{encoded},
            m_frame_number);
        readback.request = {};
        readback.submitted_frame = 0;
        readback.pending = false;
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::bind_pick_globals(
        VulkanShader& shader,
        const glm::mat4& projection,
        const glm::mat4& view) {
        CommandBuffer& commands = m_graphics_command_buffers[m_image_index];
        auto used = shader.use(commands);
        if (!used)
            return err(used.error());
        auto bound = shader.bind_globals();
        if (!bound)
            return err(bound.error());
        auto set = shader.set_uniform(
            commands, pick_projection, ShaderUniformType::mat4,
            &projection, sizeof(projection));
        if (!set)
            return err(set.error());
        set = shader.set_uniform(
            commands, pick_view, ShaderUniformType::mat4,
            &view, sizeof(view));
        if (!set)
            return err(set.error());
        return shader.apply_globals(commands, m_current_frame);
    }

    result<void, renderer_error> VulkanRenderer::draw_pick_geometry(
        VulkanShader& shader,
        u32* material_instances,
        const RenderPassKind pass,
        const GeometryRenderData& data) {
        if (!data.pick_id.valid() || data.geometry == nullptr ||
            data.geometry->material == nullptr ||
            !data.geometry->material->valid()) {
            return ok();
        }
        Material& material = *data.geometry->material;
        if (material.internal_id >= max_pick_material_count)
            return err(renderer_error{
                renderer_error_code::shader_capacity_exceeded, 0});

        u32& instance = material_instances[material.internal_id];
        if (instance == numeric::invalid_id) {
            auto acquired = shader.acquire_resources();
            if (!acquired)
                return err(acquired.error());
            instance = *acquired;
        }

        CommandBuffer& commands = m_graphics_command_buffers[m_image_index];
        auto used = shader.use(commands, material.blend_mode);
        if (!used)
            return err(used.error());
        auto bound = shader.bind_instance(instance);
        if (!bound)
            return err(bound.error());
        auto set = shader.set_uniform(
            commands, pick_diffuse_color, ShaderUniformType::f32_4,
            &material.diffuse_color, sizeof(material.diffuse_color));
        if (!set)
            return err(set.error());
        TextureBinding diffuse = material.diffuse_map.binding();
        if (diffuse.texture == nullptr || !diffuse.texture->valid())
            diffuse.texture = m_default_texture;
        set = shader.set_sampler(pick_diffuse_texture, diffuse);
        if (!set)
            return err(set.error());
        const u32 blend_mode = static_cast<u32>(material.blend_mode);
        set = shader.set_uniform(
            commands, pick_blend_mode, ShaderUniformType::u32,
            &blend_mode, sizeof(blend_mode));
        if (!set)
            return err(set.error());
        set = shader.set_uniform(
            commands, pick_alpha_cutoff, ShaderUniformType::f32,
            &material.alpha_cutoff, sizeof(material.alpha_cutoff));
        if (!set)
            return err(set.error());
        auto applied = shader.apply_instance(commands, m_current_frame, true);
        if (!applied)
            return err(applied.error());
        set = shader.set_uniform(
            commands, pick_model, ShaderUniformType::mat4,
            &data.model, sizeof(data.model));
        if (!set)
            return err(set.error());
        set = shader.set_uniform(
            commands, pick_identity, ShaderUniformType::u32,
            &data.pick_id.value, sizeof(data.pick_id.value));
        if (!set)
            return err(set.error());
        draw_geometry(pass, data);
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::draw_pick_text(
        const TextFrame& text) {
        if (text.batches.empty())
            return ok();
        CommandBuffer& commands = m_graphics_command_buffers[m_image_index];
        auto used = m_pick_text_shader.use(commands);
        if (!used)
            return err(used.error());
        auto bound = m_pick_text_shader.bind_globals();
        if (!bound)
            return err(bound.error());
        const glm::mat4 projection = glm::ortho(
            0.0f,
            static_cast<f32>(text.width),
            static_cast<f32>(text.height),
            0.0f,
            -1.0f,
            1.0f);
        auto set = m_pick_text_shader.set_uniform(
            commands,
            pick_text_projection,
            ShaderUniformType::mat4,
            &projection,
            sizeof(projection));
        if (!set)
            return err(set.error());
        auto applied = m_pick_text_shader.apply_globals(
            commands, m_current_frame);
        if (!applied)
            return err(applied.error());

        const Buffer& vertices = m_text_frames[m_current_frame].vertices;
        const vk::GraphicsCommands graphics{m_device, commands};
        for (const TextBatch& batch : text.batches) {
            if (!batch.pick_id.valid())
                continue;
            if (batch.page >= max_pick_text_page_count ||
                batch.page >= text.pages.length()) {
                return err(renderer_error{
                    renderer_error_code::shader_capacity_exceeded, 0});
            }
            u32& instance = m_pick_text_instances[batch.page];
            if (instance == numeric::invalid_id) {
                auto acquired = m_pick_text_shader.acquire_resources();
                if (!acquired)
                    return err(acquired.error());
                instance = *acquired;
            }
            bound = m_pick_text_shader.bind_instance(instance);
            if (!bound)
                return err(bound.error());
            set = m_pick_text_shader.set_sampler(
                pick_text_atlas, text.pages[batch.page].texture);
            if (!set)
                return err(set.error());
            applied = m_pick_text_shader.apply_instance(
                commands, m_current_frame, true);
            if (!applied)
                return err(applied.error());
            set = m_pick_text_shader.set_uniform(
                commands,
                pick_text_identity,
                ShaderUniformType::u32,
                &batch.pick_id.value,
                sizeof(batch.pick_id.value));
            if (!set)
                return err(set.error());
            graphics.set_scissor({
                {
                    static_cast<i32>(batch.scissor.x),
                    static_cast<i32>(batch.scissor.y),
                },
                {batch.scissor.width, batch.scissor.height},
            });
            graphics.draw(
                {
                    vertices.get(),
                    static_cast<u64>(batch.first_vertex) * sizeof(TextVertex),
                    static_cast<u64>(batch.vertex_count) * sizeof(TextVertex),
                },
                batch.vertex_count);
        }
        graphics.set_scissor({
            {0, 0},
            {m_framebuffer_width, m_framebuffer_height},
        });
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::draw_pick_frame(
        const RenderViewPacket& world,
        const RenderViewPacket& ui,
        const TextFrame* text,
        const PickRequest& request) {
        if (!m_picking_enabled || !request.valid() ||
            m_current_frame >= m_pick_readbacks.length() ||
            !m_pick_target.current()) {
            return ok();
        }

        CommandBuffer& command_buffer =
            m_graphics_command_buffers[m_image_index];
        const vk::GraphicsCommands commands{m_device, command_buffer};
        TextureData* color = static_cast<TextureData*>(
            m_pick_color.m_internal_data);
        TextureData* depth = static_cast<TextureData*>(
            m_pick_depth.m_internal_data);
        if (m_device.dynamic_rendering()) {
            if (m_pick_color_in_transfer) {
                commands.transition(
                    color->image.get(),
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                    vk::ImageUse::transfer_source,
                    vk::ImageUse::color_attachment);
                commands.transition(
                    depth->image.get(),
                    {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
                    vk::ImageUse::depth_attachment,
                    vk::ImageUse::depth_attachment);
            }
            VkClearColorValue clear{};
            clear.uint32[0] = 0;
            commands.begin_rendering({
                .color = color->image.get_view(),
                .depth = depth->image.get_view(),
                .area = m_pick_render_pass.get_render_area(),
                .clear_color = clear,
                .clear_depth = {1.0f, 0},
                .color_load = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .color_store = VK_ATTACHMENT_STORE_OP_STORE,
                .depth_load = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .depth_store = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            });
            command_buffer.set_state(CommandBufferState::InRenderPass);
        } else {
            if (m_pick_color_in_transfer) {
                commands.barrier(
                    {
                        VK_PIPELINE_STAGE_TRANSFER_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_TRANSFER_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    },
                    {
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    });
            }
            m_pick_render_pass.begin(command_buffer, m_pick_framebuffer);
        }

        bool pass_open = true;
        const auto close_pass = [&]() noexcept {
            if (!pass_open)
                return;
            if (m_device.dynamic_rendering()) {
                commands.end_rendering();
                command_buffer.set_state(CommandBufferState::Recording);
            } else {
                m_pick_render_pass.end(command_buffer);
            }
            pass_open = false;
        };
        const auto fail = [&](const renderer_error error)
            -> result<void, renderer_error> {
            close_pass();
            return err(error);
        };

        auto world_globals = bind_pick_globals(
            m_pick_world_shader, world.projection, world.view);
        if (!world_globals)
            return fail(world_globals.error());
        for (u32 index = 0; index < world.geometry_count; ++index) {
            auto drawn = draw_pick_geometry(
                m_pick_world_shader,
                m_pick_world_material_instances,
                RenderPassKind::world,
                world.geometries[index]);
            if (!drawn)
                return fail(drawn.error());
        }

        auto ui_globals = bind_pick_globals(
            m_pick_ui_shader, ui.projection, ui.view);
        if (!ui_globals)
            return fail(ui_globals.error());
        for (u32 index = 0; index < ui.geometry_count; ++index) {
            auto drawn = draw_pick_geometry(
                m_pick_ui_shader,
                m_pick_ui_material_instances,
                RenderPassKind::ui,
                ui.geometries[index]);
            if (!drawn)
                return fail(drawn.error());
        }
        if (text != nullptr) {
            auto text_drawn = draw_pick_text(*text);
            if (!text_drawn)
                return fail(text_drawn.error());
        }

        close_pass();
        if (m_device.dynamic_rendering()) {
            commands.transition(
                color->image.get(),
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                vk::ImageUse::color_attachment,
                vk::ImageUse::transfer_source);
        } else {
            // The legacy render pass final layout is transfer source.
        }

        PickReadback& readback = m_pick_readbacks[m_current_frame];
        commands.buffer_barrier(
            {readback.buffer.get(), 0, sizeof(u32)},
            {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT},
            {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT});
        commands.copy_image_to_buffer({
            .image = color->image.get(),
            .destination = {readback.buffer.get(), 0, sizeof(u32)},
            .x = static_cast<i32>(request.position.x),
            .y = static_cast<i32>(request.position.y),
        });
        commands.buffer_barrier(
            {readback.buffer.get(), 0, sizeof(u32)},
            {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT},
            {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT});
        readback.request = request;
        readback.submitted_frame = m_frame_number;
        readback.pending = false;
        m_recorded_pick_slot = static_cast<i32>(m_current_frame);
        m_pick_color_in_transfer = true;
        return ok();
    }
}
