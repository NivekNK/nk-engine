#pragma once

#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

namespace nk {
    struct DirectionalLight {
        glm::vec3 direction{-0.57735026f, -0.57735026f, -0.57735026f};
        glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};
    };

    struct SceneLighting {
        glm::vec4 ambient_color{0.25f, 0.25f, 0.25f, 1.0f};
        DirectionalLight directional{};
    };
}
