#pragma once

#include <glm/ext/matrix_float4x4.hpp>

#include "collections/dyarr.h"
#include "collections/slice.h"
#include "core/result.h"
#include "resources/geometry.h"

namespace nk {
    class GeometrySystem;
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
            cl::slice<const GeometryConfig> configs,
            glm::mat4 model = glm::mat4{1.0f});

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

        [[nodiscard]] const glm::mat4& model() const noexcept {
            return m_model;
        }
        void set_model(const glm::mat4& model) noexcept { m_model = model; }

    private:
        GeometrySystem* m_geometry_system = nullptr;
        cl::dyarr<Geometry*> m_geometries;
        glm::mat4 m_model{1.0f};
    };
}
