#pragma once

#include <cmath>

#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include "core/defines.h"

namespace nk::math {
    [[nodiscard]] inline glm::vec3 safe_normalize(
        const glm::vec3& value) noexcept {
        constexpr f32 minimum_component = 1.0e-20f;
        f32 largest_component = std::abs(value.x);
        if (std::abs(value.y) > largest_component)
            largest_component = std::abs(value.y);
        if (std::abs(value.z) > largest_component)
            largest_component = std::abs(value.z);
        if (!std::isfinite(largest_component) ||
            largest_component <= minimum_component) {
            return glm::vec3{0.0f};
        }
        const glm::vec3 scaled = value / largest_component;
        const f32 length_squared = glm::dot(scaled, scaled);
        return scaled * (1.0f / std::sqrt(length_squared));
    }

    [[nodiscard]] inline glm::mat4 normal_matrix(
        const glm::mat4& model) noexcept {
        constexpr f32 minimum_scaled_determinant = 1.0e-8f;
        const glm::mat3 linear{model};
        f32 largest_component = 0.0f;
        for (u32 column = 0; column < 3; ++column) {
            for (u32 row = 0; row < 3; ++row) {
                const f32 component = std::abs(linear[column][row]);
                if (component > largest_component)
                    largest_component = component;
            }
        }
        if (!std::isfinite(largest_component) || largest_component == 0.0f)
            return glm::mat4{0.0f};

        const f32 scaled_determinant =
            glm::determinant(linear / largest_component);
        if (!std::isfinite(scaled_determinant) ||
            std::abs(scaled_determinant) <= minimum_scaled_determinant) {
            return glm::mat4{0.0f};
        }
        const glm::mat3 inverse_transpose =
            glm::transpose(glm::inverse(linear));
        glm::mat4 result{0.0f};
        for (u32 column = 0; column < 3; ++column) {
            for (u32 row = 0; row < 3; ++row) {
                if (!std::isfinite(inverse_transpose[column][row]))
                    return glm::mat4{0.0f};
                result[column][row] = inverse_transpose[column][row];
            }
        }
        result[3][3] = 1.0f;
        return result;
    }

    [[nodiscard]] inline glm::vec3 transform_normal(
        const glm::mat4& model,
        const glm::vec3& normal) noexcept {
        return safe_normalize(glm::mat3{normal_matrix(model)} * normal);
    }
}
