#pragma once

#include "collections/dyarr.h"
#include "collections/slice.h"
#include "core/result.h"
#include "math/frustum.h"
#include "renderer/geometry_render_data.h"

namespace nk {
    enum class world_draw_list_error : u8 {
        invalid_input,
        not_initialized,
        out_of_memory,
    };

    struct WorldDrawListStats final {
        u32 candidates = 0;
        u32 visible = 0;
        u32 culled = 0;
    };

    // Filters and partitions a flattened world submission for one view.
    // Opaque geometry preserves submission order; transparent geometry is
    // stably sorted back-to-front using that view's camera position.
    [[nodiscard]] result<WorldDrawListStats, world_draw_list_error>
    build_world_draw_list(
        cl::slice<const GeometryRenderData> candidates,
        const math::Frustum& frustum,
        glm::vec3 view_position,
        bool culling_enabled,
        cl::dyarr<GeometryRenderData>& destination);
}
