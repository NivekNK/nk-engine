#include "nkpch.h"

#include "renderer/world_draw_list.h"

#include "resources/material.h"

#include <cmath>
#include <glm/geometric.hpp>

namespace nk {
    namespace {
        bool transparent(const Geometry* geometry) noexcept {
            return geometry != nullptr && geometry->material != nullptr &&
                   geometry->material->valid() &&
                   geometry->material->type == MaterialType::world &&
                   geometry->material->blend_mode ==
                       MaterialBlendMode::transparent;
        }

        bool visible(
            const GeometryRenderData& data,
            const math::Frustum& frustum,
            const bool culling_enabled) noexcept {
            if (!culling_enabled || data.geometry == nullptr)
                return true;
            const math::Aabb local = math::Aabb::from_min_max(
                data.geometry->min_extents,
                data.geometry->max_extents);
            return frustum.intersects(local.transformed(data.model));
        }

        f32 distance_squared(
            const GeometryRenderData& data,
            const glm::vec3 view_position) noexcept {
            const glm::vec3 local_center = data.geometry == nullptr
                ? glm::vec3{0.0f}
                : data.geometry->center;
            const glm::vec4 homogeneous =
                data.model * glm::vec4{local_center, 1.0f};
            if (!std::isfinite(homogeneous.x) ||
                !std::isfinite(homogeneous.y) ||
                !std::isfinite(homogeneous.z) ||
                !std::isfinite(homogeneous.w) ||
                std::abs(homogeneous.w) <= 1.0e-20f) {
                return 0.0f;
            }
            const glm::vec3 center = glm::vec3{homogeneous} / homogeneous.w;
            const glm::vec3 offset = center - view_position;
            const f32 distance = glm::dot(offset, offset);
            return std::isfinite(distance) ? distance : 0.0f;
        }
    }

    result<WorldDrawListStats, world_draw_list_error>
    build_world_draw_list(
        const cl::slice<const GeometryRenderData> candidates,
        const math::Frustum& frustum,
        const glm::vec3 view_position,
        const bool culling_enabled,
        cl::dyarr<GeometryRenderData>& destination) {
        if (destination.allocator() == nullptr)
            return err(world_draw_list_error::not_initialized);
        if (candidates.length() > numeric::u32_max)
            return err(world_draw_list_error::invalid_input);

        u64 visible_count = 0;
        u64 opaque_count = 0;
        for (const GeometryRenderData& candidate : candidates) {
            if (!visible(candidate, frustum, culling_enabled))
                continue;
            ++visible_count;
            opaque_count += transparent(candidate.geometry) ? 0 : 1;
        }

        auto reserved = destination.dyarr_reserve(visible_count);
        if (!reserved || !destination.dyarr_resize(visible_count))
            return err(world_draw_list_error::out_of_memory);

        u64 opaque_index = 0;
        u64 transparent_index = opaque_count;
        for (const GeometryRenderData& candidate : candidates) {
            if (!visible(candidate, frustum, culling_enabled))
                continue;
            destination[transparent(candidate.geometry)
                    ? transparent_index++
                    : opaque_index++] = candidate;
        }

        // Stable insertion sort keeps submission order at equal distances.
        for (u64 index = opaque_count + 1; index < visible_count; ++index) {
            GeometryRenderData value = destination[index];
            const f32 value_distance = distance_squared(value, view_position);
            u64 insert_at = index;
            while (insert_at > opaque_count &&
                   value_distance > distance_squared(
                       destination[insert_at - 1],
                       view_position)) {
                destination[insert_at] = destination[insert_at - 1];
                --insert_at;
            }
            destination[insert_at] = value;
        }

        return ok(WorldDrawListStats{
            .candidates = static_cast<u32>(candidates.length()),
            .visible = static_cast<u32>(visible_count),
            .culled = static_cast<u32>(candidates.length() - visible_count),
        });
    }
}
