#pragma once

#include "core/result.h"
#include "resources/static_mesh_resource.h"

namespace nk::mesh_binary {
    inline constexpr u32 version = 1;
    inline constexpr u32 importer_revision = 1;
    inline constexpr u32 header_bytes = 64;
    inline constexpr u32 vertex_bytes = 48;
    inline constexpr u64 max_file_bytes = 128ull * 1024 * 1024;
    inline constexpr u32 max_geometries = 4096;
    inline constexpr u32 max_materials = 4096;
    inline constexpr u32 max_dependencies = 512;
    inline constexpr u32 max_vertices = 2 * 1024 * 1024;
    inline constexpr u32 max_indices = 8 * 1024 * 1024;

    enum class error : i32 {
        none,
        truncated,
        invalid_header,
        incompatible_version,
        limit_exceeded,
        checksum_mismatch,
        invalid_data,
        out_of_memory,
    };

    enum class DependencyKind : u32 { content, presence };

    struct Dependency {
        strbuf<511> path; // Asset-root-relative, never an absolute path.
        DependencyKind kind = DependencyKind::content;
        bool present = false;
        u64 size = 0;
        u64 digest = 0;
    };

    struct Archive {
        StaticMeshResource mesh;
        cl::dyarr<Dependency> dependencies;
    };

    [[nodiscard]] bool safe_relative_path(strview path) noexcept;
    [[nodiscard]] result<cl::dyarr<u8>, error> encode(
        mem::Allocator& allocator, const StaticMeshResource& mesh,
        cl::slice<const Dependency> dependencies = {});
    // Validates the entire archive before allocating its runtime payload.
    // The input is borrowed only for this call and must remain immutable.
    [[nodiscard]] result<Archive, error> decode(
        mem::Allocator& allocator, cl::slice<const u8> bytes);
}
