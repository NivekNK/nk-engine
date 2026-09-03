#include "geometry_render_data.h"
#include "nkpch.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
#include "vulkan/vulkan_renderer.h"
#include "platform/platform.h"
#include "systems/material_system.h"

#include <glm/gtc/matrix_transform.hpp>

namespace nk {
    result<Renderer*, renderer_error> Renderer::create(
        mem::Allocator* allocator,
        Platform* platform,
        ResourceSystem* resources,
        const strview application_name) {
        if (allocator == nullptr || platform == nullptr || resources == nullptr)
            std::abort();

        auto renderer = allocator->construct_t(
            VulkanRenderer,
            *allocator,
            resources,
            application_name);
        if (renderer == nullptr)
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });

        renderer->m_platform = platform;

        renderer->m_allocator = native_construct(mem::MallocAllocator);
        if (renderer->m_allocator == nullptr) {
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        }
        if (renderer->m_allocator->allocator_init(
                mem::MallocAllocator,
                "Renderer",
                MemoryType::Renderer) == nullptr) {
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(renderer_error{
                .code = renderer_error_code::initialization_failed,
                .native_code = 0,
            });
        }

        renderer->m_frame_number = 0;

        renderer->m_near_clip = 0.1f;
        renderer->m_far_clip = 1000.0f;

        f32 aspect = platform->width() / static_cast<f32>(platform->height());
        renderer->m_projection = glm::perspective(
            glm::radians(45.0f), aspect, renderer->m_near_clip, renderer->m_far_clip);
        renderer->m_view = glm::inverse(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -30.0f)));
        renderer->m_ui_projection = glm::ortho(
            0.0f,
            static_cast<f32>(platform->width()),
            static_cast<f32>(platform->height()),
            0.0f,
            -100.0f,
            100.0f);
        renderer->m_ui_view = glm::mat4(1.0f);

        auto initialized = renderer->init();
        if (!initialized) {
            const renderer_error error = initialized.error();
            renderer->shutdown();
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(error);
        }

        return ok(static_cast<Renderer*>(renderer));
    }

    void Renderer::destroy(mem::Allocator* allocator, Renderer* renderer) {
        if (allocator == nullptr || renderer == nullptr)
            return;
        renderer->shutdown();
        native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
        allocator->deconstruct_t(VulkanRenderer, renderer);
    }

    result<frame_outcome, renderer_error> Renderer::draw_frame(
        MaterialSystem& materials,
        const RenderPacket& packet) {
        auto begun = begin_frame(packet.delta_time);
        if (!begun)
            return err(begun.error());
        if (*begun == frame_outcome::skipped_swapchain_recreation)
            return ok(frame_outcome::skipped_swapchain_recreation);

        auto world_drawn = draw_render_pass(
            materials,
            RenderPassKind::world,
            m_projection,
            m_view,
            packet.geometry_count,
            packet.geometries);
        if (!world_drawn)
            return err(world_drawn.error());

        auto ui_drawn = draw_render_pass(
            materials,
            RenderPassKind::ui,
            m_ui_projection,
            m_ui_view,
            packet.ui_geometry_count,
            packet.ui_geometries);
        if (!ui_drawn)
            return err(ui_drawn.error());

        return end_frame_impl(packet.delta_time);
    }

    result<void, renderer_error> Renderer::draw_render_pass(
        MaterialSystem& materials,
        const RenderPassKind pass,
        const glm::mat4& projection,
        const glm::mat4& view,
        const u32 geometry_count,
        const GeometryRenderData* geometries) {
        begin_render_pass(pass);
        auto fail = [this, pass](const renderer_error error)
            -> result<void, renderer_error> {
            end_render_pass(pass);
            return err(error);
        };

        const MaterialType expected_material_type =
            pass == RenderPassKind::world
                ? MaterialType::world
                : MaterialType::ui;
        auto globals_applied = materials.apply_global(
            expected_material_type,
            projection,
            view);
        if (!globals_applied)
            return fail({
                renderer_error_code::material_failed,
                globals_applied.error().native_code,
            });

        for (u32 index = 0; index < geometry_count; ++index) {
            const GeometryRenderData& data = geometries[index];
            Material* material = data.geometry == nullptr
                ? nullptr
                : data.geometry->material;
            if (material == nullptr || !material->valid() ||
                material->type != expected_material_type) {
                material = expected_material_type == MaterialType::world
                    ? &materials.default_material()
                    : &materials.default_ui_material();
            }

            auto instance_applied = materials.apply_instance(
                *material,
                m_frame_number);
            if (!instance_applied) {
                return fail({
                    renderer_error_code::material_failed,
                    instance_applied.error().native_code,
                });
            }
            auto local_applied = materials.apply_local(*material, data.model);
            if (!local_applied) {
                return fail({
                    renderer_error_code::material_failed,
                    local_applied.error().native_code,
                });
            }
            draw_geometry(pass, data);
        }
        end_render_pass(pass);
        return ok();
    }

    void Renderer::resize(u32 width, u32 height) {
        m_projection = glm::perspective(
            glm::radians(45.0f), width / static_cast<f32>(height), m_near_clip, m_far_clip);
        m_ui_projection = glm::ortho(
            0.0f,
            static_cast<f32>(width),
            static_cast<f32>(height),
            0.0f,
            -100.0f,
            100.0f);
        on_resized(width, height);
    }

    result<frame_outcome, renderer_error> Renderer::end_frame_impl(
        const f64 delta_time) {
        auto ended = end_frame(delta_time);
        if (!ended)
            return err(ended.error());
        if (*ended == frame_outcome::rendered)
            ++m_frame_number;
        return ended;
    }
}
