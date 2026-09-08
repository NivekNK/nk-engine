#pragma once

#include "collections/dyarr.h"
#include "core/strbuf.h"
#include "resources/material.h"
#include "vendor/glm/vertex_2d.h"
#include "vendor/glm/vertex_3d.h"

namespace nk {
    inline constexpr u64 geometry_name_capacity = 255;

    struct GeometryConfig {
        cl::dyarr<glm::Vertex3D> vertices;
        cl::dyarr<u32> indices;
        strbuf<geometry_name_capacity> name;
        strbuf<material_name_capacity> material_name;
        glm::vec3 center{0.0f};
        glm::vec3 min_extents{0.0f};
        glm::vec3 max_extents{0.0f};
    };

    struct Geometry2DConfig {
        cl::dyarr<glm::Vertex2D> vertices;
        cl::dyarr<u32> indices;
        strbuf<geometry_name_capacity> name;
        strbuf<material_name_capacity> material_name;
    };

    struct Geometry {
        u32 id = numeric::invalid_id;
        u32 internal_id = numeric::invalid_id;
        u32 generation = numeric::invalid_id;
        strbuf<geometry_name_capacity> name;
        Material* material = nullptr;
        glm::vec3 center{0.0f};
        glm::vec3 min_extents{0.0f};
        glm::vec3 max_extents{0.0f};

        bool valid() const noexcept {
            return internal_id != numeric::invalid_id &&
                   generation != numeric::invalid_id;
        }
    };
}
