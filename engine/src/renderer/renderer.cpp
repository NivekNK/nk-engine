#include "geometry_render_data.h"
#include "nkpch.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
#include "vulkan/vulkan_renderer.h"
#include "platform/platform.h"
#include "systems/event_system.h"
#include "systems/material_system.h"
#include "resources/mesh.h"
#include "renderer/text_renderer.h"
#include "renderer/world_draw_list.h"

#include <glm/gtc/matrix_transform.hpp>

namespace nk {
    Renderer::~Renderer() {
        m_pick_registry.shutdown();
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
        const cstr culling = std::getenv("NK_FRUSTUM_CULLING");
        renderer->m_frustum_culling_enabled =
            culling == nullptr || std::strcmp(culling, "0") != 0;

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

        auto picking_initialized = renderer->m_pick_registry.init(
            *renderer->m_allocator);
        if (!picking_initialized) {
            renderer->m_render_views.shutdown();
            renderer->release_world_draw_scratch();
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            renderer->m_allocator = nullptr;
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = static_cast<i32>(picking_initialized.error()),
            });
        }

        auto initialized = renderer->init();
        if (!initialized) {
            const renderer_error error = initialized.error();
            renderer->shutdown();
            renderer->m_pick_registry.shutdown();
            renderer->m_render_views.shutdown();
            renderer->release_world_draw_scratch();
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            renderer->m_allocator = nullptr;
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(error);
        }

        InfoLog(
            "World frustum culling {}.",
            renderer->m_frustum_culling_enabled ? "enabled" : "disabled");

        return ok(static_cast<Renderer*>(renderer));
    }

    void Renderer::destroy(mem::Allocator* allocator, Renderer* renderer) {
        if (allocator == nullptr || renderer == nullptr)
            return;
        renderer->shutdown();
        renderer->m_pick_registry.shutdown();
        renderer->m_render_views.shutdown();
        renderer->release_world_draw_scratch();
        native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
        renderer->m_allocator = nullptr;
        allocator->deconstruct_t(VulkanRenderer, renderer);
    }

    result<frame_outcome, renderer_error> Renderer::draw_frame(
        MaterialSystem& materials,
        const RenderPacket& packet) {
        m_frame_draw_counters = {};
        auto prepared = prepare_world_draws(packet);
        if (!prepared)
            return err(prepared.error());

        auto begun = begin_frame(packet.delta_time);
        if (!begun)
            return err(begun.error());
        if (*begun == frame_outcome::skipped_swapchain_recreation)
            return ok(frame_outcome::skipped_swapchain_recreation);

        if (packet.text) {
            auto text_prepared = prepare_text_frame(*packet.text);
            if (!text_prepared) return err(text_prepared.error());
        }

        RenderViewPacket view_packets[
            RenderViewSystem::maximum_render_view_count]{};
        auto built = m_render_views.build_packets({
            .world_view = &m_view,
            .world_view_position = m_view_position,
            .lighting = &packet.lighting,
            .geometry_count = static_cast<u32>(
                m_world_draw_scratch.length()),
            .geometries = m_world_draw_scratch.data(),
            .skybox_geometry = packet.skybox_geometry,
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
        u32 world_view_index = 0;
        for (u32 index = 0; index < *built; ++index) {
            if (view_packets[index].type != RenderViewType::world)
                continue;
            auto prepared_view = prepare_world_view_draws(
                view_packets[index],
                world_view_index++);
            if (!prepared_view)
                return err(prepared_view.error());
        }
        for (u32 index = 0; index < *built; ++index) {
            auto drawn = draw_render_pass(materials, view_packets[index], packet.text);
            if (!drawn)
                return err(drawn.error());
        }


        PickRequest pick_request{};
        if (picking_enabled() && m_pick_queue.consume(pick_request)) {
            pick_request.submitted_frame = m_frame_number;
            const RenderViewPacket* world = nullptr;
            const RenderViewPacket* ui = nullptr;
            for (u32 index = 0; index < *built; ++index) {
                if (view_packets[index].type == RenderViewType::world)
                    world = &view_packets[index];
                else if (view_packets[index].type == RenderViewType::ui)
                    ui = &view_packets[index];
            }
            if (world != nullptr && ui != nullptr) {
                auto picked = draw_pick_frame(
                    *world, *ui, packet.text, pick_request);
                if (!picked)
                    return err(picked.error());
            }
        }

        return end_frame_impl(packet.delta_time);
    }

    result<u64, pick_error> Renderer::request_pick(
        const f32 logical_x,
        const f32 logical_y,
        const f32 content_scale,
        const u64 scene_revision,
        const PickRequestKind kind) noexcept {
        auto position = physical_pick_position(
            logical_x,
            logical_y,
            content_scale,
            m_render_views.width(),
            m_render_views.height());
        if (!position)
            return err(position.error());
        m_pick_queue.discard_scene(scene_revision);
        return m_pick_queue.submit(*position, scene_revision, kind);
    }

    void Renderer::publish_pick_sample(
        const PickRequest& request,
        const PickId id,
        const u64 retired_frame) noexcept {
        PickObject object{};
        if (id.valid()) {
            auto resolved = m_pick_registry.resolve(id);
            if (!resolved)
                return;
            object = *resolved;
        }
        m_pick_queue.publish({
            .request = request,
            .id = id,
            .object = object,
            .retired_frame = retired_frame,
        });
        if (retired_frame >= request.submitted_frame) {
            m_pick_timing = {
                .request_frame = request.submitted_frame,
                .retired_frame = retired_frame,
                .frames = static_cast<f64>(
                    retired_frame - request.submitted_frame),
            };
            m_pick_timing_pending = true;
        }
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

        u64 destination = 0;
        for (u32 index = 0; index < packet.geometry_count; ++index)
            m_world_draw_scratch[destination++] = packet.geometries[index];
        for (u32 mesh_index = 0; mesh_index < packet.mesh_count; ++mesh_index) {
            const Mesh& mesh = packet.meshes[mesh_index];
            const glm::mat4 model = mesh.transform().world_matrix();
            for (Geometry* geometry : mesh.geometries()) {
                m_world_draw_scratch[destination++] = {
                    .model = model,
                    .geometry = geometry,
                    .pick_id = mesh.pick_id(),
                };
            }
        }
        return ok();
    }

    result<void, renderer_error> Renderer::prepare_world_view_draws(
        RenderViewPacket& packet,
        const u32 world_view_index) {
        if (world_view_index >= RenderViewSystem::maximum_render_view_count) {
            return err(renderer_error{
                renderer_error_code::initialization_failed,
                0,
            });
        }
        const math::Frustum frustum = math::Frustum::from_view_projection(
            packet.projection * packet.view);
        auto prepared = build_world_draw_list(
            {packet.geometries, packet.geometry_count},
            frustum,
            packet.view_position,
            m_frustum_culling_enabled,
            m_world_view_scratch[world_view_index]);
        if (!prepared) {
            return err(renderer_error{
                prepared.error() == world_draw_list_error::out_of_memory
                    ? renderer_error_code::out_of_memory
                    : renderer_error_code::initialization_failed,
                static_cast<i32>(prepared.error()),
            });
        }
        m_frame_draw_counters.candidates += prepared->candidates;
        m_frame_draw_counters.visible += prepared->visible;
        m_frame_draw_counters.culled += prepared->culled;
        packet.geometry_count = prepared->visible;
        packet.geometries = m_world_view_scratch[world_view_index].data();
        packet.mesh_count = 0;
        packet.meshes = nullptr;
        return ok();
    }

    result<void, renderer_error> Renderer::draw_render_pass(
        MaterialSystem& materials,
        const RenderViewPacket& packet, const TextFrame* text) {
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
        Material* bound_material = nullptr;
        if (packet.type == RenderViewType::world &&
            packet.skybox_geometry.geometry != nullptr) {
            auto skybox_globals = materials.apply_global(
                MaterialType::skybox,
                packet.projection,
                packet.view,
                packet.view_position,
                *packet.lighting,
                RenderViewMode::default_lit);
            if (!skybox_globals) {
                return fail({
                    renderer_error_code::material_failed,
                    skybox_globals.error().native_code,
                });
            }
            auto skybox_drawn = draw_render_data(
                materials,
                pass,
                MaterialType::skybox,
                packet.skybox_geometry,
                bound_material);
            if (!skybox_drawn)
                return fail(skybox_drawn.error());
            bound_material = nullptr;
        }
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
        if (text && packet.type == RenderViewType::ui) {
            auto drawn = draw_text_frame(*text);
            if (!drawn) return fail(drawn.error());
            m_frame_draw_counters.draws +=
                static_cast<u32>(text->batches.length());
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
            if (expected_material_type == MaterialType::skybox) {
                return err(renderer_error{
                    renderer_error_code::material_failed,
                    0,
                });
            }
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
        ++m_frame_draw_counters.draws;
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
