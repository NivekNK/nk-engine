#pragma once

#include <glm/ext/vector_float4.hpp>

namespace nk {
    // NOTE: 256 bytes
    struct ObjectUniformObject {
        glm::vec4 diffuse_color; // 16 bytes
        glm::vec4 m_reserved0; // 16 bytes, reserved for future use
        glm::vec4 m_reserved1; // 16 bytes, reserved for future use
        glm::vec4 m_reserved2; // 16 bytes, reserved for future use
    };
}
