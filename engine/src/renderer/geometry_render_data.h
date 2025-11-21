#pragma once

#include "resources/texture.h"

#include <glm/ext/matrix_float4x4.hpp>

namespace nk {
    struct GeometryRenderData {
        u32 object_id;
        glm::mat4 model;
        Texture* texture[16];
    };
}
