#include "nkpch.h"

#include "resources/mesh.h"

#include "memory/allocator.h"
#include "resources/static_mesh_resource.h"
#include "systems/geometry_system.h"

#include <utility>

namespace nk {
    Mesh::~Mesh() {
        reset();
    }

    Mesh::Mesh(Mesh&& other) noexcept
        : m_geometry_system{other.m_geometry_system},
          m_geometries{std::move(other.m_geometries)},
          m_transform{std::move(other.m_transform)} {
        other.m_geometry_system = nullptr;
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept {
        if (this == &other)
            return *this;

        reset();
        m_geometry_system = other.m_geometry_system;
        m_geometries = std::move(other.m_geometries);
        m_transform = std::move(other.m_transform);
        other.m_geometry_system = nullptr;
        return *this;
    }

    result<Mesh, mesh_error> Mesh::create(
        mem::Allocator& allocator,
        GeometrySystem& geometries,
        const cl::slice<const GeometryConfig> configs) {
        Mesh mesh;
        mesh.m_geometry_system = &geometries;

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

    result<Mesh, mesh_error> Mesh::create(
        mem::Allocator& allocator,
        GeometrySystem& geometries,
        const StaticMeshResource& resource) {
        Mesh mesh;
        mesh.m_geometry_system = &geometries;

        if (resource.geometries.empty())
            return ok(std::move(mesh));
        if (!mesh.m_geometries.dyarr_init_len(
                &allocator,
                resource.geometries.length(),
                resource.geometries.length())) {
            return err(mesh_error{mesh_error_code::out_of_memory});
        }

        for (u64 index = 0; index < resource.geometries.length(); ++index) {
            const GeometryConfig& config = resource.geometries[index];
            const MaterialConfig* material = resource.material(
                config.material_name.view());
            auto acquired = material == nullptr
                ? geometries.acquire(config, true)
                : geometries.acquire(config, *material, true);
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
        m_transform = Transform{};
    }

    Geometry* Mesh::geometry(const u64 index) const noexcept {
        return index < m_geometries.length()
            ? m_geometries[index]
            : nullptr;
    }
}
