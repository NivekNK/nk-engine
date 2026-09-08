#pragma once

#include "renderer/lighting.h"
#include "renderer/picking.h"
#include "resources/geometry.h"

#include <glm/ext/matrix_float4x4.hpp>

namespace nk {
    class Mesh;
    struct TextFrame;

    struct GeometryRenderData {
        glm::mat4 model{1.0f};
        Geometry* geometry = nullptr;
        PickId pick_id{};
    };

    // All pointers are borrowed until Renderer::draw_frame returns. Backends
    // must copy any host data they need before retaining work asynchronously.
    struct RenderPacket {
        f64 delta_time = 0.0;
        SceneLighting lighting{};
        u32 geometry_count = 0;
        const GeometryRenderData* geometries = nullptr;
        GeometryRenderData skybox_geometry{};
        u32 mesh_count = 0;
        const Mesh* meshes = nullptr;
        u32 ui_geometry_count = 0;
        const GeometryRenderData* ui_geometries = nullptr;
        const TextFrame* text = nullptr;
    };
}
