#include "geometry_render_data.h"
#include "nkpch.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
#include "vulkan/vulkan_renderer.h"
#include "platform/platform.h"
#include "systems/event_system.h"
#include "systems/material_system.h"
#include "resources/mesh.h"

#include <glm/gtc/matrix_transform.hpp>

namespace nk {
    Renderer::~Renderer() {
        release_world_draw_scratch();
    }

    bool on_render_view_mode(
        const SystemEventCode code,
        void*,
        void* listener,
        const EventContext context) {
        if (code != SystemEventCode::SetRenderViewMode || listener == nullptr)
            return false;

        Renderer& renderer = *static_cast<Renderer*>(listener);
        const RenderViewMode mode =
            static_cast<RenderViewMode>(context.data.u32[0]);
        if (!renderer.set_render_view_mode(mode))
            return false;

        InfoLog(
            "Render view mode changed to {}.",
            static_cast<u32>(mode));
        return true;
    }

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

        if (!renderer->initialize_world_draw_scratch(
                *renderer->m_allocator)) {
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            renderer->m_allocator = nullptr;
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        }

        renderer->m_view = glm::inverse(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -30.0f)));
        renderer->m_view_position = glm::vec3(0.0f, 0.0f, -30.0f);
        auto views_initialized = renderer->m_render_views.init(
            *renderer->m_allocator,
            platform->width(),
            platform->height());
        if (!views_initialized) {
            renderer->release_world_draw_scratch();
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            renderer->m_allocator = nullptr;
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(renderer_error{
                .code = renderer_error_code::initialization_failed,
                .native_code = static_cast<i32>(views_initialized.error()),
            });
        }

        auto initialized = renderer->init();
        if (!initialized) {
            const renderer_error error = initialized.error();
            renderer->shutdown();
            renderer->m_render_views.shutdown();
            renderer->release_world_draw_scratch();
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            renderer->m_allocator = nullptr;
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(error);
        }

        return ok(static_cast<Renderer*>(renderer));
    }

    void Renderer::destroy(mem::Allocator* allocator, Renderer* renderer) {
        if (allocator == nullptr || renderer == nullptr)
            return;
        renderer->shutdown();
        renderer->m_render_views.shutdown();
        renderer->release_world_draw_scratch();
        native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
        renderer->m_allocator = nullptr;
        allocator->deconstruct_t(VulkanRenderer, renderer);
    }

    result<frame_outcome, renderer_error> Renderer::draw_frame(
        MaterialSystem& materials,
        const RenderPacket& packet) {
        auto prepared = prepare_world_draws(packet);
        if (!prepared)
            return err(prepared.error());

        auto begun = begin_frame(packet.delta_time);
        if (!begun)
            return err(begun.error());
        if (*begun == frame_outcome::skipped_swapchain_recreation)
            return ok(frame_outcome::skipped_swapchain_recreation);

        RenderViewPacket view_packets[
            RenderViewSystem::maximum_render_view_count]{};
        auto built = m_render_views.build_packets({
            .world_view = &m_view,
            .world_view_position = m_view_position,
            .lighting = &packet.lighting,
            .geometry_count = static_cast<u32>(
                m_world_draw_scratch.length()),
            .geometries = m_world_draw_scratch.data(),
            .mesh_count = 0,
            .meshes = nullptr,
            .ui_geometry_count = packet.ui_geometry_count,
            .ui_geometries = packet.ui_geometries,
        }, {view_packets});
        if (!built) {
            return err(renderer_error{
                renderer_error_code::initialization_failed,
                static_cast<i32>(built.error()),
            });
        }
        for (u32 index = 0; index < *built; ++index) {
            auto drawn = draw_render_pass(materials, view_packets[index]);
            if (!drawn)
                return err(drawn.error());
        }

        return end_frame_impl(packet.delta_time);
    }

    result<void, renderer_error> Renderer::prepare_world_draws(
        const RenderPacket& packet) {
        if ((packet.geometry_count != 0 && packet.geometries == nullptr) ||
            (packet.mesh_count != 0 && packet.meshes == nullptr)) {
            return err(renderer_error{
                renderer_error_code::initialization_failed,
                0,
            });
        }

        u64 total = packet.geometry_count;
        for (u32 mesh_index = 0; mesh_index < packet.mesh_count; ++mesh_index) {
            const u64 count = packet.meshes[mesh_index].geometries().length();
            if (count > numeric::u32_max - total) {
                return err(renderer_error{
                    renderer_error_code::out_of_memory,
                    0,
                });
            }
            total += count;
        }
        if (m_world_draw_scratch.allocator() == nullptr &&
            !m_world_draw_scratch.dyarr_init(m_allocator, total)) {
            return err(renderer_error{
                renderer_error_code::out_of_memory,
                0,
            });
        }
        auto reserved = m_world_draw_scratch.dyarr_reserve(total);
        if (!reserved || !m_world_draw_scratch.dyarr_resize(total)) {
            return err(renderer_error{
                renderer_error_code::out_of_memory,
                0,
            });
        }

        auto transparent = [](const Geometry* geometry) noexcept {
            return geometry != nullptr && geometry->material != nullptr &&
                   geometry->material->valid() &&
                   geometry->material->type == MaterialType::world &&
                   geometry->material->blend_mode ==
                       MaterialBlendMode::transparent;
        };
        u64 opaque_count = 0;
        for (u32 index = 0; index < packet.geometry_count; ++index)
            opaque_count += transparent(packet.geometries[index].geometry) ? 0 : 1;
        for (u32 mesh_index = 0; mesh_index < packet.mesh_count; ++mesh_index) {
            for (Geometry* geometry : packet.meshes[mesh_index].geometries())
                opaque_count += transparent(geometry) ? 0 : 1;
        }

        u64 opaque_index = 0;
        u64 transparent_index = opaque_count;
        auto append = [&](const GeometryRenderData data) {
            m_world_draw_scratch[
                transparent(data.geometry)
                    ? transparent_index++
                    : opaque_index++] = data;
        };
        for (u32 index = 0; index < packet.geometry_count; ++index)
            append(packet.geometries[index]);
        for (u32 mesh_index = 0; mesh_index < packet.mesh_count; ++mesh_index) {
            const Mesh& mesh = packet.meshes[mesh_index];
            const glm::mat4 model = mesh.transform().world_matrix();
            for (Geometry* geometry : mesh.geometries())
                append({.model = model, .geometry = geometry});
        }

        auto distance_squared = [this](const GeometryRenderData& data) {
            const glm::vec3 local_center = data.geometry == nullptr
                ? glm::vec3{0.0f}
                : data.geometry->center;
            const glm::vec3 center = glm::vec3{
                data.model * glm::vec4{local_center, 1.0f}};
            const glm::vec3 offset = center - m_view_position;
            const f32 distance = glm::dot(offset, offset);
            return std::isfinite(distance) ? distance : 0.0f;
        };
        // Stable insertion sort keeps submission order for equal distances.
        for (u64 index = opaque_count + 1; index < total; ++index) {
            GeometryRenderData value = m_world_draw_scratch[index];
            const f32 value_distance = distance_squared(value);
            u64 destination = index;
            while (destination > opaque_count &&
                   value_distance > distance_squared(
                       m_world_draw_scratch[destination - 1])) {
                m_world_draw_scratch[destination] =
                    m_world_draw_scratch[destination - 1];
                --destination;
            }
            m_world_draw_scratch[destination] = value;
        }
        return ok();
    }

    result<void, renderer_error> Renderer::draw_render_pass(
        MaterialSystem& materials,
        const RenderViewPacket& packet) {
        const RenderPassKind pass = packet.pass;
        begin_render_pass(pass);
        auto fail = [this, pass](const renderer_error error)
            -> result<void, renderer_error> {
            end_render_pass(pass);
            return err(error);
        };

        const MaterialType expected_material_type =
            packet.type == RenderViewType::world
                ? MaterialType::world
                : MaterialType::ui;
        auto globals_applied = materials.apply_global(
            expected_material_type,
            packet.projection,
            packet.view,
            packet.view_position,
            *packet.lighting,
            packet.type == RenderViewType::world
                ? m_render_view_mode
                : RenderViewMode::default_lit);
        if (!globals_applied)
            return fail({
                renderer_error_code::material_failed,
                globals_applied.error().native_code,
            });

        Material* bound_material = nullptr;
        for (u32 index = 0; index < packet.geometry_count; ++index) {
            auto drawn = draw_render_data(
                materials,
                pass,
                expected_material_type,
                packet.geometries[index],
                bound_material);
            if (!drawn)
                return fail({
                    renderer_error_code::material_failed,
                    drawn.error().native_code,
                });
        }
        for (u32 mesh_index = 0; mesh_index < packet.mesh_count; ++mesh_index) {
            const Mesh& mesh = packet.meshes[mesh_index];
            for (Geometry* geometry : mesh.geometries()) {
                auto drawn = draw_render_data(
                    materials,
                    pass,
                    expected_material_type,
                    {
                        .model = mesh.transform().world_matrix(),
                        .geometry = geometry,
                    },
                    bound_material);
                if (!drawn)
                    return fail(drawn.error());
            }
        }
        end_render_pass(pass);
        return ok();
    }

    result<void, renderer_error> Renderer::draw_render_data(
        MaterialSystem& materials,
        const RenderPassKind pass,
        const MaterialType expected_material_type,
        const GeometryRenderData data,
        Material*& bound_material) {
        Material* material = data.geometry == nullptr
            ? nullptr
            : data.geometry->material;
        if (material == nullptr || !material->valid() ||
            material->type != expected_material_type) {
            material = expected_material_type == MaterialType::world
                ? &materials.default_material()
                : &materials.default_ui_material();
        }

        if (bound_material != material) {
            auto blend_selected = set_material_blend_mode(
                material->blend_mode);
            if (!blend_selected)
                return err(blend_selected.error());
            auto instance_applied = materials.apply_instance(
                *material,
                m_frame_number);
            if (!instance_applied) {
                return err(renderer_error{
                    renderer_error_code::material_failed,
                    instance_applied.error().native_code,
                });
            }
            bound_material = material;
        }
        auto local_applied = materials.apply_local(*material, data.model);
        if (!local_applied) {
            return err(renderer_error{
                renderer_error_code::material_failed,
                local_applied.error().native_code,
            });
        }
        draw_geometry(pass, data);
        return ok();
    }

    void Renderer::resize(u32 width, u32 height) {
        auto resized = m_render_views.resize(width, height);
        if (!resized)
            return;
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
