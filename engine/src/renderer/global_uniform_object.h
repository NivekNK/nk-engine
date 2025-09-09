#pragma once

#include <glm/ext/matrix_float4x4.hpp>

namespace nk {
    // NOTE: 256 bytes
    struct GlobalUniformObject {
        glm::mat4 projection;  // 64 bytes
        glm::mat4 view;        // 64 bytes
        glm::mat4 m_reserved0; // 64 bytes, reserved for future use
        glm::mat4 m_reserved1; // 64 bytes, reserved for future use
    };
}
