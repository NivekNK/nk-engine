#include "nkpch.h"

#include "renderer/render_view.h"

#include "memory/allocator.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace nk {
    namespace {
        u32 next_generation(const u32 generation) noexcept {
            if (generation == numeric::invalid_id)
                return 0;
            const u32 next = generation + 1;
            return next == numeric::invalid_id ? 0 : next;
        }

        bool valid_borrowed_range(const u32 count, const void* data) noexcept {
            return count == 0 || data != nullptr;
        }
    }

    RenderViewSystem::~RenderViewSystem() {
        shutdown();
    }

    result<void, render_view_error> RenderViewSystem::init(
        mem::Allocator& allocator,
        const u32 width,
        const u32 height,
        const u32 max_view_count) {
        if (m_initialized)
            return err(render_view_error::already_initialized);
        if (width == 0 || height == 0)
            return err(render_view_error::invalid_extent);
        if (max_view_count < 2 || max_view_count > maximum_render_view_count)
            return err(render_view_error::capacity_exceeded);

        m_allocator = &allocator;
        if (!m_views.arr_init(&allocator, max_view_count)) {
            shutdown();
            return err(render_view_error::out_of_memory);
        }
        auto handles = m_handles.map_init(
            &allocator,
            max_view_count,
            hash_seed::deterministic);
        if (!handles) {
            shutdown();
            return err(render_view_error::out_of_memory);
        }
        m_width = width;
        m_height = height;
        m_initialized = true;

        auto world = create({
            .name = world_render_view_name,
            .type = RenderViewType::world,
            .pass = RenderPassKind::world,
            .projection = ProjectionType::perspective,
            .order = 1,
        });
        if (!world) {
            const render_view_error error = world.error();
            shutdown();
            return err(error);
        }
        auto ui = create({
            .name = ui_render_view_name,
            .type = RenderViewType::ui,
            .pass = RenderPassKind::ui,
            .projection = ProjectionType::orthographic,
            .order = 2,
            .near_clip = -100.0f,
            .far_clip = 100.0f,
        });
        if (!ui) {
            const render_view_error error = ui.error();
            shutdown();
            return err(error);
        }
        return ok();
    }

    void RenderViewSystem::shutdown() noexcept {
        m_initialized = false;
        m_view_count = 0;
        m_width = 0;
        m_height = 0;
        if (m_handles.allocator() != nullptr)
            (void)m_handles.map_shutdown();
        if (m_views.allocator() != nullptr)
            (void)m_views.arr_shutdown();
        m_allocator = nullptr;
    }

    result<RenderViewHandle, render_view_error> RenderViewSystem::create(
        const RenderViewConfig& config) {
        if (!m_initialized)
            return err(render_view_error::not_initialized);
        if (config.name.empty())
            return err(render_view_error::invalid_name);
        if (!valid_config(config))
            return err(render_view_error::invalid_configuration);
        if (m_handles.find(config.name) != nullptr)
            return err(render_view_error::invalid_name);
        for (const ViewSlot& existing : m_views) {
            if (existing.occupied && existing.config.order == config.order)
                return err(render_view_error::duplicate_order);
        }

        const u32 slot_index = find_free_slot();
        if (slot_index == numeric::invalid_id)
            return err(render_view_error::capacity_exceeded);
        ViewSlot& destination = m_views[slot_index];
        const RenderViewHandle view_handle{
            .index = slot_index,
            .generation = destination.generation == numeric::invalid_id
                ? 0
                : destination.generation,
        };

        str owned_name{*m_allocator};
        if (!owned_name.assign(config.name))
            return err(render_view_error::out_of_memory);
        auto inserted = m_handles.insert(std::move(owned_name), view_handle);
        if (!inserted)
            return err(render_view_error::out_of_memory);

        destination.config = config;
        destination.config.name = {};
        destination.projection = projection_for(config, m_width, m_height);
        destination.generation = view_handle.generation;
        destination.occupied = true;
        ++m_view_count;
        return ok(view_handle);
    }

    result<void, render_view_error> RenderViewSystem::remove(
        const RenderViewHandle view_handle) {
        if (!m_initialized)
            return err(render_view_error::not_initialized);
        const ViewSlot* existing = slot(view_handle);
        if (existing == nullptr)
            return err(render_view_error::invalid_handle);

        strview registered_name{};
        for (auto entry : m_handles) {
            if (entry.value == view_handle) {
                registered_name = entry.key.view();
                break;
            }
        }
        if (registered_name.empty() || !m_handles.remove(registered_name))
            return err(render_view_error::invalid_handle);

        ViewSlot& destination = m_views[view_handle.index];
        destination.config = {};
        destination.projection = glm::mat4{1.0f};
        destination.generation = next_generation(destination.generation);
        destination.occupied = false;
        --m_view_count;
        return ok();
    }

    result<void, render_view_error> RenderViewSystem::resize(
        const u32 width,
        const u32 height) noexcept {
        if (!m_initialized)
            return err(render_view_error::not_initialized);
        if (width == 0 || height == 0)
            return err(render_view_error::invalid_extent);

        for (ViewSlot& view : m_views) {
            if (view.occupied)
                view.projection = projection_for(view.config, width, height);
        }
        m_width = width;
        m_height = height;
        return ok();
    }

    result<u32, render_view_error> RenderViewSystem::build_packets(
        const RenderViewBuildData& input,
        const cl::slice<RenderViewPacket> output) const noexcept {
        if (!m_initialized)
            return err(render_view_error::not_initialized);
        if (input.world_view == nullptr || input.lighting == nullptr ||
            !valid_borrowed_range(input.geometry_count, input.geometries) ||
            !valid_borrowed_range(input.mesh_count, input.meshes) ||
            !valid_borrowed_range(
                input.ui_geometry_count,
                input.ui_geometries)) {
            return err(render_view_error::invalid_build_data);
        }
        if (output.length() < m_view_count)
            return err(render_view_error::output_too_small);

        const ViewSlot* ordered[maximum_render_view_count]{};
        u32 ordered_count = 0;
        for (const ViewSlot& view : m_views) {
            if (!view.occupied)
                continue;
            u32 insert_at = ordered_count;
            while (insert_at > 0 &&
                   ordered[insert_at - 1]->config.order > view.config.order) {
                ordered[insert_at] = ordered[insert_at - 1];
                --insert_at;
            }
            ordered[insert_at] = &view;
            ++ordered_count;
        }

        for (u32 index = 0; index < ordered_count; ++index) {
            const ViewSlot& source = *ordered[index];
            RenderViewPacket packet{
                .handle = {
                    .index = static_cast<u32>(&source - m_views.data()),
                    .generation = source.generation,
                },
                .type = source.config.type,
                .pass = source.config.pass,
                .projection = source.projection,
                .lighting = input.lighting,
            };
            if (source.config.type == RenderViewType::world) {
                packet.view = *input.world_view;
                packet.view_position = input.world_view_position;
                packet.geometry_count = input.geometry_count;
                packet.geometries = input.geometries;
                packet.mesh_count = input.mesh_count;
                packet.meshes = input.meshes;
            } else {
                packet.view = glm::mat4{1.0f};
                packet.geometry_count = input.ui_geometry_count;
                packet.geometries = input.ui_geometries;
            }
            output[index] = packet;
        }
        return ok(ordered_count);
    }

    RenderViewHandle RenderViewSystem::handle(const strview name) const noexcept {
        if (!m_initialized)
            return {};
        const RenderViewHandle* found = m_handles.find(name);
        return found == nullptr ? RenderViewHandle{} : *found;
    }

    bool RenderViewSystem::valid(const RenderViewHandle handle) const noexcept {
        return slot(handle) != nullptr;
    }

    bool RenderViewSystem::valid_config(
        const RenderViewConfig& config) noexcept {
        if (!std::isfinite(config.near_clip) ||
            !std::isfinite(config.far_clip) ||
            config.near_clip == config.far_clip) {
            return false;
        }
        switch (config.type) {
            case RenderViewType::world:
                return config.pass == RenderPassKind::world &&
                       config.projection == ProjectionType::perspective &&
                       std::isfinite(config.field_of_view_radians) &&
                       config.field_of_view_radians > 0.0f &&
                       config.field_of_view_radians < 3.1415926535897932f &&
                       config.near_clip > 0.0f &&
                       config.far_clip > config.near_clip;
            case RenderViewType::ui:
                return config.pass == RenderPassKind::ui &&
                       config.projection == ProjectionType::orthographic;
        }
        return false;
    }

    glm::mat4 RenderViewSystem::projection_for(
        const RenderViewConfig& config,
        const u32 width,
        const u32 height) noexcept {
        if (config.projection == ProjectionType::perspective) {
            return glm::perspective(
                config.field_of_view_radians,
                width / static_cast<f32>(height),
                config.near_clip,
                config.far_clip);
        }
        return glm::ortho(
            0.0f,
            static_cast<f32>(width),
            static_cast<f32>(height),
            0.0f,
            config.near_clip,
            config.far_clip);
    }

    u32 RenderViewSystem::find_free_slot() const noexcept {
        for (u32 index = 0; index < m_views.length(); ++index) {
            if (!m_views[index].occupied)
                return index;
        }
        return numeric::invalid_id;
    }

    const RenderViewSystem::ViewSlot* RenderViewSystem::slot(
        const RenderViewHandle handle) const noexcept {
        if (!m_initialized || handle.index >= m_views.length())
            return nullptr;
        const ViewSlot& candidate = m_views[handle.index];
        if (!candidate.occupied || candidate.generation != handle.generation)
            return nullptr;
        return &candidate;
    }
}
