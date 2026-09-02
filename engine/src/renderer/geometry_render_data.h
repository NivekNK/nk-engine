#pragma once

#include "resources/material.h"

#include <glm/ext/matrix_float4x4.hpp>

namespace nk {
    struct GeometryRenderData {
        glm::mat4 model;
        Material* material;
    };
}
