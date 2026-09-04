#pragma once

#include "collections/dyarr.h"
#include "collections/slice.h"
#include "resources/geometry.h"
#include "resources/material.h"

namespace nk {
    enum class static_mesh_parse_error : i32 {
        parser_failed = 1,
        empty_mesh,
        too_many_geometries,
        invalid_face,
        invalid_index,
        invalid_number,
        name_too_long,
    };

    struct StaticMeshResource final {
        cl::dyarr<GeometryConfig> geometries;
        cl::dyarr<MaterialConfig> materials;

        [[nodiscard]] cl::slice<const GeometryConfig> geometry_configs()
            const noexcept {
            return {geometries.data(), geometries.length()};
        }

        [[nodiscard]] const MaterialConfig* material(
            const strview name) const noexcept {
            for (const MaterialConfig& config : materials) {
                if (config.name.view() == name)
                    return &config;
            }
            return nullptr;
        }
    };
}
