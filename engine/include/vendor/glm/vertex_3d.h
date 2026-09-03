#pragma once

#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float2.hpp>

namespace glm {
    struct Vertex3D {
        vec3 position;
        vec3 normal;
        vec2 texcoord;
    };
}
