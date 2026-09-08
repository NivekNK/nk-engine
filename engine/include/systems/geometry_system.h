#pragma once

#include "collections/arr.h"
#include "core/result.h"
#include "resources/geometry.h"

namespace nk {
    class MaterialSystem;
    class Renderer;
    namespace mem { class Allocator; }

    inline constexpr strview default_geometry_name{"default", 7};
    inline constexpr strview default_ui_geometry_name{"default_ui", 10};

    enum class geometry_error_code : u8 {
        invalid_config,
        invalid_id,
        not_initialized,
        capacity_exceeded,
        out_of_memory,
        material_failed,
        renderer_failed,
    };

    struct geometry_error {
        geometry_error_code code;
        i32 native_code;
    };

    class GeometrySystem final {
    public:
        static constexpr u32 default_max_geometry_count = 4096;

        GeometrySystem() = default;
        ~GeometrySystem();

        GeometrySystem(const GeometrySystem&) = delete;
        GeometrySystem& operator=(const GeometrySystem&) = delete;
        GeometrySystem(GeometrySystem&&) = delete;
        GeometrySystem& operator=(GeometrySystem&&) = delete;

        [[nodiscard]] static result<GeometrySystem*, geometry_error> create(
            mem::Allocator& allocator,
            Renderer& renderer,
            MaterialSystem& materials,
            u32 max_geometry_count = default_max_geometry_count);
        static void destroy(mem::Allocator& allocator, GeometrySystem* system);

        [[nodiscard]] result<Geometry*, geometry_error> acquire(u32 id);
        [[nodiscard]] result<Geometry*, geometry_error> acquire(
            const GeometryConfig& config,
            bool auto_release);
        [[nodiscard]] result<Geometry*, geometry_error> acquire(
            const GeometryConfig& config,
            const MaterialConfig& material,
            bool auto_release);
        [[nodiscard]] result<Geometry*, geometry_error> acquire(
            const Geometry2DConfig& config,
            bool auto_release);
        void release(Geometry* geometry);

        Geometry& default_geometry() noexcept { return m_default_geometry; }
        const Geometry& default_geometry() const noexcept {
            return m_default_geometry;
        }
        Geometry& default_ui_geometry() noexcept {
            return m_default_ui_geometry;
        }
        const Geometry& default_ui_geometry() const noexcept {
            return m_default_ui_geometry;
        }

        [[nodiscard]] static result<GeometryConfig, geometry_error>
        generate_plane(
            mem::Allocator& allocator,
            f32 width,
            f32 height,
            u32 x_segment_count,
            u32 y_segment_count,
            f32 tile_x,
            f32 tile_y,
            strview name,
            strview material_name);

        [[nodiscard]] static result<GeometryConfig, geometry_error>
        generate_cube(
            mem::Allocator& allocator,
            f32 width,
            f32 height,
            f32 depth,
            f32 tile_x,
            f32 tile_y,
            strview name,
            strview material_name);

        [[nodiscard]] static result<void, geometry_error> generate_normals(
            cl::slice<glm::Vertex3D> vertices,
            cl::slice<const u32> indices);
        [[nodiscard]] static result<void, geometry_error> generate_tangents(
            mem::Allocator& allocator,
            cl::slice<glm::Vertex3D> vertices,
            cl::slice<const u32> indices);

        u32 loaded_count() const noexcept { return m_loaded_count; }
        u64 reference_count(u32 id) const noexcept;

    private:
        struct GeometryReference {
            u64 reference_count = 0;
            Geometry geometry{};
            bool auto_release = false;
        };

        [[nodiscard]] result<void, geometry_error> init(
            mem::Allocator& allocator,
            Renderer& renderer,
            MaterialSystem& materials,
            u32 max_geometry_count);
        void shutdown();
        [[nodiscard]] result<void, geometry_error> create_default_geometries();
        template<typename Vertex>
        [[nodiscard]] result<Geometry*, geometry_error> acquire_geometry(
            cl::slice<const Vertex> vertices,
            cl::slice<const u32> indices,
            strview name,
            strview material_name,
            MaterialType material_type,
            const MaterialConfig* material_config,
            glm::vec3 center,
            glm::vec3 min_extents,
            glm::vec3 max_extents,
            bool auto_release);
        template<typename Vertex>
        [[nodiscard]] result<void, geometry_error> create_geometry(
            cl::slice<const Vertex> vertices,
            cl::slice<const u32> indices,
            strview name,
            strview material_name,
            MaterialType material_type,
            const MaterialConfig* material_config,
            Geometry& geometry);
        void destroy_geometry(Geometry& geometry);
        u32 find_free_slot() const noexcept;

        mem::Allocator* m_allocator = nullptr;
        Renderer* m_renderer = nullptr;
        MaterialSystem* m_materials = nullptr;
        cl::arr<GeometryReference> m_geometries;
        Geometry m_default_geometry{};
        Geometry m_default_ui_geometry{};
        u32 m_loaded_count = 0;
        bool m_initialized = false;
    };
}
