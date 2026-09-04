#include "nkpch.h"

#include "resources/mesh.h"

#include "memory/allocator.h"
#include "systems/geometry_system.h"

#include <utility>

namespace nk {
    Mesh::~Mesh() {
        reset();
    }

    Mesh::Mesh(Mesh&& other) noexcept
        : m_geometry_system{other.m_geometry_system},
          m_geometries{std::move(other.m_geometries)},
          m_model{other.m_model} {
        other.m_geometry_system = nullptr;
        other.m_model = glm::mat4{1.0f};
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept {
        if (this == &other)
            return *this;

        reset();
        m_geometry_system = other.m_geometry_system;
        m_geometries = std::move(other.m_geometries);
        m_model = other.m_model;
        other.m_geometry_system = nullptr;
        other.m_model = glm::mat4{1.0f};
        return *this;
    }

    result<Mesh, mesh_error> Mesh::create(
        mem::Allocator& allocator,
        GeometrySystem& geometries,
        const cl::slice<const GeometryConfig> configs,
        const glm::mat4 model) {
        Mesh mesh;
        mesh.m_geometry_system = &geometries;
        mesh.m_model = model;

        if (configs.empty())
            return ok(std::move(mesh));

        if (!mesh.m_geometries.dyarr_init_len(
                &allocator,
                configs.length(),
                configs.length())) {
            return err(mesh_error{mesh_error_code::out_of_memory});
        }

        for (u64 index = 0; index < configs.length(); ++index) {
            auto acquired = geometries.acquire(configs[index], true);
            if (!acquired) {
                const geometry_error error = acquired.error();
                return err(mesh_error{
                    mesh_error_code::geometry_failed,
                    static_cast<u32>(index),
                    static_cast<u32>(error.code),
                    error.native_code,
                });
            }
            mesh.m_geometries[index] = *acquired;
        }

        return ok(std::move(mesh));
    }

    void Mesh::reset() noexcept {
        if (m_geometry_system != nullptr) {
            for (Geometry* geometry : m_geometries)
                m_geometry_system->release(geometry);
        }
        if (m_geometries.allocator() != nullptr)
            (void)m_geometries.dyarr_shutdown();
        m_geometry_system = nullptr;
        m_model = glm::mat4{1.0f};
    }

    Geometry* Mesh::geometry(const u64 index) const noexcept {
        return index < m_geometries.length()
            ? m_geometries[index]
            : nullptr;
    }
}
