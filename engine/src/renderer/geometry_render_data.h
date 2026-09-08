#pragma once

#include "resources/geometry.h"
#include "renderer/picking.h"

#include <glm/ext/matrix_float4x4.hpp>

namespace nk {
    struct GeometryRenderData {
        glm::mat4 model;
        Geometry* geometry;
        PickId pick_id{};
    };
}
