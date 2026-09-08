#include "nkpch.h"

#include "math/frustum.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace nk::math {
    namespace {
        bool finite(const glm::vec3 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                   std::isfinite(value.z);
        }

        bool finite(const glm::vec4 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                   std::isfinite(value.z) && std::isfinite(value.w);
        }

        bool finite(const glm::mat4& value) noexcept {
            for (u32 column = 0; column < 4; ++column) {
                for (u32 row = 0; row < 4; ++row) {
                    if (!std::isfinite(value[column][row]))
                        return false;
                }
            }
            return true;
        }

        glm::vec4 row(const glm::mat4& matrix, const u32 index) noexcept {
            return {
                matrix[0][index],
                matrix[1][index],
                matrix[2][index],
                matrix[3][index],
            };
        }
    }

    Plane Plane::from_coefficients(const glm::vec4 coefficients) noexcept {
        if (!math::finite(coefficients))
            return {};
        const glm::vec3 raw_normal{coefficients};
        const f32 length = glm::length(raw_normal);
        if (!std::isfinite(length) || length <= 1.0e-20f)
            return {};
        const f32 inverse_length = 1.0f / length;
        return {
            .normal = raw_normal * inverse_length,
            .distance = coefficients.w * inverse_length,
            .finite = true,
        };
    }

    f32 Plane::signed_distance(const glm::vec3 point) const noexcept {
        return glm::dot(normal, point) + distance;
    }

    Aabb Aabb::from_min_max(
        const glm::vec3 minimum,
        const glm::vec3 maximum) noexcept {
        if (!math::finite(minimum) || !math::finite(maximum))
            return {};
        const glm::vec3 ordered_minimum = glm::min(minimum, maximum);
        const glm::vec3 ordered_maximum = glm::max(minimum, maximum);
        return {
            .center = (ordered_minimum + ordered_maximum) * 0.5f,
            .half_extents = (ordered_maximum - ordered_minimum) * 0.5f,
            .finite = true,
        };
    }

    Aabb Aabb::transformed(const glm::mat4& model) const noexcept {
        if (!finite || !math::finite(model))
            return {};

        const glm::vec4 transformed_center = model * glm::vec4{center, 1.0f};
        if (!math::finite(transformed_center) ||
            std::abs(transformed_center.w) <= 1.0e-20f) {
            return {};
        }
        const glm::vec3 world_center =
            glm::vec3{transformed_center} / transformed_center.w;
        glm::vec3 world_half_extents{0.0f};
        for (u32 world_axis = 0; world_axis < 3; ++world_axis) {
            for (u32 local_axis = 0; local_axis < 3; ++local_axis) {
                world_half_extents[world_axis] +=
                    std::abs(model[local_axis][world_axis]) *
                    half_extents[local_axis];
            }
        }
        if (!math::finite(world_center) ||
            !math::finite(world_half_extents)) {
            return {};
        }
        return {
            .center = world_center,
            .half_extents = world_half_extents,
            .finite = true,
        };
    }

    Frustum Frustum::from_view_projection(
        const glm::mat4& view_projection) noexcept {
        if (!math::finite(view_projection))
            return {};

        const glm::vec4 x = row(view_projection, 0);
        const glm::vec4 y = row(view_projection, 1);
        const glm::vec4 z = row(view_projection, 2);
        const glm::vec4 w = row(view_projection, 3);
        Frustum result{};
        result.planes[static_cast<u32>(FrustumSide::left)] =
            Plane::from_coefficients(w + x);
        result.planes[static_cast<u32>(FrustumSide::right)] =
            Plane::from_coefficients(w - x);
        result.planes[static_cast<u32>(FrustumSide::bottom)] =
            Plane::from_coefficients(w + y);
        result.planes[static_cast<u32>(FrustumSide::top)] =
            Plane::from_coefficients(w - y);
        result.planes[static_cast<u32>(FrustumSide::near)] =
            Plane::from_coefficients(w + z);
        result.planes[static_cast<u32>(FrustumSide::far)] =
            Plane::from_coefficients(w - z);
        result.finite = true;
        for (const Plane& plane : result.planes)
            result.finite = result.finite && plane.finite;
        return result;
    }

    bool Frustum::contains(const glm::vec3 point) const noexcept {
        if (!finite || !math::finite(point))
            return true;
        for (const Plane& plane : planes) {
            if (plane.signed_distance(point) < 0.0f)
                return false;
        }
        return true;
    }

    bool Frustum::intersects_sphere(
        const glm::vec3 center,
        const f32 radius) const noexcept {
        if (!finite || !math::finite(center) || !std::isfinite(radius) ||
            radius < 0.0f) {
            return true;
        }
        for (const Plane& plane : planes) {
            if (plane.signed_distance(center) + radius < 0.0f)
                return false;
        }
        return true;
    }

    bool Frustum::intersects(const Aabb& bounds) const noexcept {
        if (!finite || !bounds.finite)
            return true;
        for (const Plane& plane : planes) {
            const f32 projected_radius =
                glm::dot(glm::abs(plane.normal), bounds.half_extents);
            if (plane.signed_distance(bounds.center) + projected_radius < 0.0f)
                return false;
        }
        return true;
    }
}
