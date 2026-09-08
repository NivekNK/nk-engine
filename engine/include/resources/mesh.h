#pragma once

#include "collections/dyarr.h"
#include "collections/slice.h"
#include "core/result.h"
#include "core/transform.h"
#include "resources/geometry.h"
#include "renderer/picking.h"

namespace nk {
    class GeometrySystem;
    struct StaticMeshResource;
    namespace mem { class Allocator; }

    enum class mesh_error_code : u8 {
        out_of_memory,
        geometry_failed,
    };

    struct mesh_error {
        mesh_error_code code;
        u32 geometry_index = numeric::invalid_id;
        u32 geometry_error = numeric::invalid_id;
        i32 native_code = 0;
    };

    // Owns one GeometrySystem acquisition for every stored geometry. Geometry
    // pointers returned by this type are borrowed views and remain stable only
    // while the Mesh is alive. The GeometrySystem must outlive the Mesh.
    class Mesh final {
    public:
        Mesh() = default;
        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;

        [[nodiscard]] static result<Mesh, mesh_error> create(
            mem::Allocator& allocator,
            GeometrySystem& geometries,
            cl::slice<const GeometryConfig> configs);
        [[nodiscard]] static result<Mesh, mesh_error> create(
            mem::Allocator& allocator,
            GeometrySystem& geometries,
            const StaticMeshResource& resource);

        void reset() noexcept;

        [[nodiscard]] bool valid() const noexcept {
            return m_geometry_system != nullptr;
        }
        [[nodiscard]] u64 geometry_count() const noexcept {
            return m_geometries.length();
        }
        [[nodiscard]] Geometry* geometry(u64 index) const noexcept;
        [[nodiscard]] cl::slice<Geometry* const> geometries() const noexcept {
            return {m_geometries.data(), m_geometries.length()};
        }

        [[nodiscard]] Transform& transform() noexcept {
            return m_transform;
        }
        [[nodiscard]] const Transform& transform() const noexcept {
            return m_transform;
        }
        void set_pick_id(const PickId id) noexcept { m_pick_id = id; }
        [[nodiscard]] PickId pick_id() const noexcept { return m_pick_id; }

    private:
        GeometrySystem* m_geometry_system = nullptr;
        cl::dyarr<Geometry*> m_geometries;
        Transform m_transform;
        PickId m_pick_id{};
    };
}
