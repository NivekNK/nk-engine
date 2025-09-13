#pragma once

#include <glm/ext/matrix_float4x4.hpp>

namespace nk {
    // It needs to be 256 bytes aligned
    struct GlobalUniformObject {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 m_reserved0;
        glm::mat4 m_reserved1;
    };
}
