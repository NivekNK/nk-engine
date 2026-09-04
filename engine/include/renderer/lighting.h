#pragma once

#include <cmath>

#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

namespace nk {
    inline constexpr u32 max_point_light_count = 2;

    enum class RenderViewMode : u32 {
        default_lit = 0,
        lighting_only = 1,
        normals = 2,
    };

    [[nodiscard]] constexpr bool valid_render_view_mode(
        const RenderViewMode mode) noexcept {
        return static_cast<u32>(mode) <=
            static_cast<u32>(RenderViewMode::normals);
    }

    struct DirectionalLight {
        glm::vec3 direction{-0.57735026f, -0.57735026f, -0.57735026f};
        glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};
    };

    struct PointLight {
        glm::vec3 position{};
        glm::vec4 color{1.0f};
        f32 constant = 1.0f;
        f32 linear = 0.0f;
        f32 quadratic = 0.0f;

        [[nodiscard]] bool valid() const noexcept {
            return std::isfinite(position.x) &&
                std::isfinite(position.y) &&
                std::isfinite(position.z) &&
                std::isfinite(color.r) && std::isfinite(color.g) &&
                std::isfinite(color.b) && std::isfinite(color.a) &&
                std::isfinite(constant) && constant > 0.0f &&
                std::isfinite(linear) && linear >= 0.0f &&
                std::isfinite(quadratic) && quadratic >= 0.0f;
        }

        [[nodiscard]] f32 attenuation(const f32 distance) const noexcept {
            if (!valid() || !std::isfinite(distance) || distance < 0.0f)
                return 0.0f;
            const f32 denominator = constant + linear * distance +
                quadratic * distance * distance;
            return std::isfinite(denominator) && denominator > 0.0f
                ? 1.0f / denominator
                : 0.0f;
        }
    };

    struct alignas(16) PointLightUniform {
        glm::vec3 position{};
        f32 constant = 1.0f;
        glm::vec4 color{};
        f32 linear = 0.0f;
        f32 quadratic = 0.0f;
        glm::vec2 padding{};

        PointLightUniform() = default;
        explicit PointLightUniform(const PointLight& light) noexcept
            : position{light.position},
              constant{light.constant},
              color{light.color},
              linear{light.linear},
              quadratic{light.quadratic} {}
    };

    struct SceneLighting {
        glm::vec4 ambient_color{0.25f, 0.25f, 0.25f, 1.0f};
        DirectionalLight directional{};
        PointLight point_lights[max_point_light_count]{};
        u32 point_light_count = 0;
    };
}
