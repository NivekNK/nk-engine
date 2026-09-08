#pragma once

#include "core/defines.h"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

namespace nk::math {
    struct Plane final {
        glm::vec3 normal{0.0f};
        f32 distance = 0.0f;
        bool finite = false;

        [[nodiscard]] static Plane from_coefficients(
            glm::vec4 coefficients) noexcept;
        [[nodiscard]] f32 signed_distance(glm::vec3 point) const noexcept;
    };

    struct Aabb final {
        glm::vec3 center{0.0f};
        glm::vec3 half_extents{0.0f};
        bool finite = false;

        [[nodiscard]] static Aabb from_min_max(
            glm::vec3 minimum,
            glm::vec3 maximum) noexcept;
        [[nodiscard]] Aabb transformed(const glm::mat4& model) const noexcept;
    };

    enum class FrustumSide : u8 {
        left,
        right,
        bottom,
        top,
        near,
        far,
        count,
    };

    struct Frustum final {
        static constexpr u32 plane_count =
            static_cast<u32>(FrustumSide::count);

        Plane planes[plane_count]{};
        bool finite = false;

        // Extracts the OpenGL-style clip volume currently produced by GLM:
        // -w <= x,y,z <= w. A depth [0,w] migration is intentionally separate.
        [[nodiscard]] static Frustum from_view_projection(
            const glm::mat4& view_projection) noexcept;
        [[nodiscard]] bool contains(glm::vec3 point) const noexcept;
        [[nodiscard]] bool intersects_sphere(
            glm::vec3 center,
            f32 radius) const noexcept;
        [[nodiscard]] bool intersects(const Aabb& bounds) const noexcept;
    };
}
