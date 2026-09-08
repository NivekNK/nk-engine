#include "nkpch.h"

#include "systems/geometry_system.h"

#include "core/math.h"
#include "memory/allocator.h"
#include "renderer/renderer.h"
#include "systems/material_system.h"

#include <cmath>

#include <glm/geometric.hpp>

namespace nk {
    GeometrySystem::~GeometrySystem() {
        shutdown();
    }

    result<GeometrySystem*, geometry_error> GeometrySystem::create(
        mem::Allocator& allocator,
        Renderer& renderer,
        MaterialSystem& materials,
        const u32 max_geometry_count) {
        GeometrySystem* system = allocator.construct_t(GeometrySystem);
        if (system == nullptr)
            return err(geometry_error{geometry_error_code::out_of_memory, 0});

        auto initialized = system->init(
            allocator,
            renderer,
            materials,
            max_geometry_count);
        if (!initialized) {
            const geometry_error error = initialized.error();
            allocator.deconstruct_t(GeometrySystem, system);
            return err(error);
        }
        return ok(system);
    }

    void GeometrySystem::destroy(
        mem::Allocator& allocator,
        GeometrySystem* system) {
        if (system != nullptr)
            allocator.deconstruct_t(GeometrySystem, system);
    }

    result<void, geometry_error> GeometrySystem::init(
        mem::Allocator& allocator,
        Renderer& renderer,
        MaterialSystem& materials,
        const u32 max_geometry_count) {
        if (max_geometry_count == 0) {
            return err(geometry_error{
                geometry_error_code::capacity_exceeded,
                0,
            });
        }

        m_allocator = &allocator;
        m_renderer = &renderer;
        m_materials = &materials;
        if (!m_geometries.arr_init(&allocator, max_geometry_count)) {
            shutdown();
            return err(geometry_error{geometry_error_code::out_of_memory, 0});
        }

        auto default_created = create_default_geometries();
        if (!default_created) {
            const geometry_error error = default_created.error();
            shutdown();
            return err(error);
        }

        m_initialized = true;
        InfoLog(
            "Geometry system initialized with capacity {}.",
            max_geometry_count);
        return ok();
    }

    void GeometrySystem::shutdown() {
        if (m_renderer != nullptr) {
            for (GeometryReference& reference : m_geometries) {
                if (reference.geometry.valid())
                    destroy_geometry(reference.geometry);
            }
            if (m_default_ui_geometry.valid())
                destroy_geometry(m_default_ui_geometry);
            if (m_default_geometry.valid())
                destroy_geometry(m_default_geometry);
        }

        if (m_geometries.allocator() != nullptr)
            (void)m_geometries.arr_shutdown();

        m_default_geometry = {};
        m_default_ui_geometry = {};
        m_loaded_count = 0;
        m_initialized = false;
        m_materials = nullptr;
        m_renderer = nullptr;
        m_allocator = nullptr;
    }

    result<void, geometry_error> GeometrySystem::create_default_geometries() {
        glm::Vertex3D vertices[4]{};
        constexpr f32 scale = 10.0f;
        vertices[0] = {
            .position = {-0.5f * scale, -0.5f * scale, 0.0f},
            .normal = {},
            .texcoord = {0.0f, 0.0f},
            .tangent = {},
        };
        vertices[1] = {
            .position = {0.5f * scale, 0.5f * scale, 0.0f},
            .normal = {},
            .texcoord = {1.0f, 1.0f},
            .tangent = {},
        };
        vertices[2] = {
            .position = {-0.5f * scale, 0.5f * scale, 0.0f},
            .normal = {},
            .texcoord = {0.0f, 1.0f},
            .tangent = {},
        };
        vertices[3] = {
            .position = {0.5f * scale, -0.5f * scale, 0.0f},
            .normal = {},
            .texcoord = {1.0f, 0.0f},
            .tangent = {},
        };
        const u32 indices[6]{0, 1, 2, 0, 3, 1};
        auto normals_generated = generate_normals(vertices, indices);
        if (!normals_generated)
            return err(normals_generated.error());
        auto tangents_generated = generate_tangents(
            *m_allocator,
            vertices,
            indices);
        if (!tangents_generated)
            return err(tangents_generated.error());

        Geometry world{};
        world.name.assign(default_geometry_name);
        world.material = &m_materials->default_material();
        world.center = {0.0f, 0.0f, 0.0f};
        world.min_extents = {-0.5f * scale, -0.5f * scale, 0.0f};
        world.max_extents = {0.5f * scale, 0.5f * scale, 0.0f};
        auto world_created = m_renderer->create_geometry(
            world,
            cl::slice<const glm::Vertex3D>{vertices},
            cl::slice<const u32>{indices});
        if (!world_created) {
            return err(geometry_error{
                geometry_error_code::renderer_failed,
                world_created.error().native_code,
            });
        }
        world.id = numeric::invalid_id;
        world.generation = 0;

        glm::Vertex2D ui_vertices[4]{};
        ui_vertices[0] = {
            .position = {-0.5f * scale, -0.5f * scale},
            .texcoord = {0.0f, 0.0f},
        };
        ui_vertices[1] = {
            .position = {0.5f * scale, 0.5f * scale},
            .texcoord = {1.0f, 1.0f},
        };
        ui_vertices[2] = {
            .position = {-0.5f * scale, 0.5f * scale},
            .texcoord = {0.0f, 1.0f},
        };
        ui_vertices[3] = {
            .position = {0.5f * scale, -0.5f * scale},
            .texcoord = {1.0f, 0.0f},
        };

        Geometry ui{};
        ui.name.assign(default_ui_geometry_name);
        ui.material = &m_materials->default_ui_material();
        auto ui_created = m_renderer->create_geometry(
            ui,
            cl::slice<const glm::Vertex2D>{ui_vertices},
            cl::slice<const u32>{indices});
        if (!ui_created) {
            m_renderer->destroy_geometry(world);
            return err(geometry_error{
                geometry_error_code::renderer_failed,
                ui_created.error().native_code,
            });
        }
        ui.id = numeric::invalid_id;
        ui.generation = 0;

        m_default_geometry = world;
        m_default_ui_geometry = ui;
        return ok();
    }

    result<Geometry*, geometry_error> GeometrySystem::acquire(const u32 id) {
        if (!m_initialized)
            return err(geometry_error{geometry_error_code::not_initialized, 0});
        if (id >= m_geometries.length() ||
            !m_geometries[id].geometry.valid()) {
            return err(geometry_error{geometry_error_code::invalid_id, 0});
        }

        ++m_geometries[id].reference_count;
        return ok(&m_geometries[id].geometry);
    }

    result<Geometry*, geometry_error> GeometrySystem::acquire(
        const GeometryConfig& config,
        const bool auto_release) {
        return acquire_geometry(
            cl::slice<const glm::Vertex3D>{config.vertices},
            cl::slice<const u32>{config.indices},
            config.name.view(),
            config.material_name.view(),
            MaterialType::world,
            nullptr,
            config.center,
            config.min_extents,
            config.max_extents,
            auto_release);
    }

    result<Geometry*, geometry_error> GeometrySystem::acquire(
        const GeometryConfig& config,
        const MaterialConfig& material,
        const bool auto_release) {
        if (config.material_name.view() != material.name.view()) {
            return err(geometry_error{
                geometry_error_code::invalid_config,
                0,
            });
        }
        return acquire_geometry(
            cl::slice<const glm::Vertex3D>{config.vertices},
            cl::slice<const u32>{config.indices},
            config.name.view(),
            config.material_name.view(),
            material.type,
            &material,
            config.center,
            config.min_extents,
            config.max_extents,
            auto_release);
    }

    result<Geometry*, geometry_error> GeometrySystem::acquire(
        const Geometry2DConfig& config,
        const bool auto_release) {
        return acquire_geometry(
            cl::slice<const glm::Vertex2D>{config.vertices},
            cl::slice<const u32>{config.indices},
            config.name.view(),
            config.material_name.view(),
            MaterialType::ui,
            nullptr,
            {},
            {},
            {},
            auto_release);
    }

    template<typename Vertex>
    result<Geometry*, geometry_error> GeometrySystem::acquire_geometry(
        const cl::slice<const Vertex> vertices,
        const cl::slice<const u32> indices,
        const strview name,
        const strview material_name,
        const MaterialType material_type,
        const MaterialConfig* material_config,
        const glm::vec3 center,
        const glm::vec3 min_extents,
        const glm::vec3 max_extents,
        const bool auto_release) {
        if (!m_initialized)
            return err(geometry_error{geometry_error_code::not_initialized, 0});
        if (vertices.empty()) {
            return err(geometry_error{
                geometry_error_code::invalid_config,
                0,
            });
        }

        const u32 slot = find_free_slot();
        if (slot == numeric::invalid_id) {
            return err(geometry_error{
                geometry_error_code::capacity_exceeded,
                0,
            });
        }

        Geometry geometry{};
        geometry.id = slot;
        geometry.center = center;
        geometry.min_extents = min_extents;
        geometry.max_extents = max_extents;
        auto created = create_geometry(
            vertices,
            indices,
            name,
            material_name,
            material_type,
            material_config,
            geometry);
        if (!created)
            return err(created.error());

        GeometryReference& reference = m_geometries[slot];
        reference.reference_count = 1;
        reference.auto_release = auto_release;
        reference.geometry = geometry;
        ++m_loaded_count;
        return ok(&reference.geometry);
    }

    void GeometrySystem::release(Geometry* geometry) {
        if (!m_initialized || geometry == nullptr ||
            geometry->id >= m_geometries.length()) {
            return;
        }

        GeometryReference& reference = m_geometries[geometry->id];
        if (&reference.geometry != geometry ||
            reference.reference_count == 0) {
            ErrorLog("Geometry release rejected an invalid handle.");
            return;
        }

        --reference.reference_count;
        if (reference.reference_count == 0 && reference.auto_release) {
            destroy_geometry(reference.geometry);
            reference.auto_release = false;
            --m_loaded_count;
        }
    }

    template<typename Vertex>
    result<void, geometry_error> GeometrySystem::create_geometry(
        const cl::slice<const Vertex> vertices,
        const cl::slice<const u32> indices,
        const strview name,
        const strview material_name,
        const MaterialType material_type,
        const MaterialConfig* material_config,
        Geometry& geometry) {
        const strview fallback_geometry_name =
            material_type == MaterialType::ui
                ? default_ui_geometry_name
                : default_geometry_name;
        geometry.name.assign(name.empty() ? fallback_geometry_name : name);

        auto uploaded = m_renderer->create_geometry(
            geometry,
            vertices,
            indices);
        if (!uploaded) {
            geometry = {};
            return err(geometry_error{
                geometry_error_code::renderer_failed,
                uploaded.error().native_code,
            });
        }

        Material* default_material = material_type == MaterialType::world
            ? &m_materials->default_material()
            : material_type == MaterialType::ui
                ? &m_materials->default_ui_material()
                : nullptr;
        if (material_config != nullptr) {
            if (material_config->type != material_type ||
                material_config->name.view() != material_name) {
                m_renderer->destroy_geometry(geometry);
                geometry = {};
                return err(geometry_error{
                    geometry_error_code::invalid_config,
                    0,
                });
            }
            auto material = m_materials->acquire(*material_config);
            if (!material) {
                m_renderer->destroy_geometry(geometry);
                geometry = {};
                return err(geometry_error{
                    geometry_error_code::material_failed,
                    material.error().native_code,
                });
            }
            geometry.material = *material;
        } else if (default_material != nullptr && (material_name.empty() ||
            material_name == default_material_name ||
            material_name == default_material->name.view())) {
            geometry.material = default_material;
        } else if (default_material == nullptr) {
            m_renderer->destroy_geometry(geometry);
            geometry = {};
            return err(geometry_error{
                geometry_error_code::invalid_config,
                0,
            });
        } else {
            auto material = m_materials->acquire(material_name);
            if (!material) {
                WarnLog(
                    "Unable to acquire material '{}' for geometry '{}'; using default.",
                    material_name,
                    geometry.name.view());
                geometry.material = default_material;
            } else if ((*material)->type != material_type) {
                m_materials->release((*material)->name.view());
                m_renderer->destroy_geometry(geometry);
                geometry = {};
                return err(geometry_error{
                    geometry_error_code::invalid_config,
                    0,
                });
            } else {
                geometry.material = *material;
            }
        }
        geometry.generation = 0;
        return ok();
    }

    void GeometrySystem::destroy_geometry(Geometry& geometry) {
        Material* material = geometry.material;
        m_renderer->destroy_geometry(geometry);
        if (material != nullptr &&
            material != &m_materials->default_material() &&
            material != &m_materials->default_ui_material()) {
            m_materials->release(material->name.view());
        }
        geometry = {};
    }

    u32 GeometrySystem::find_free_slot() const noexcept {
        for (u32 index = 0; index < m_geometries.length(); ++index) {
            if (!m_geometries[index].geometry.valid())
                return index;
        }
        return numeric::invalid_id;
    }

    u64 GeometrySystem::reference_count(const u32 id) const noexcept {
        if (!m_initialized || id >= m_geometries.length())
            return 0;
        return m_geometries[id].reference_count;
    }

    result<GeometryConfig, geometry_error> GeometrySystem::generate_plane(
        mem::Allocator& allocator,
        f32 width,
        f32 height,
        u32 x_segment_count,
        u32 y_segment_count,
        f32 tile_x,
        f32 tile_y,
        const strview name,
        const strview material_name) {
        if (width == 0.0f)
            width = 1.0f;
        if (height == 0.0f)
            height = 1.0f;
        if (x_segment_count == 0)
            x_segment_count = 1;
        if (y_segment_count == 0)
            y_segment_count = 1;
        if (tile_x == 0.0f)
            tile_x = 1.0f;
        if (tile_y == 0.0f)
            tile_y = 1.0f;

        const u64 segment_count =
            static_cast<u64>(x_segment_count) * y_segment_count;
        if (segment_count > numeric::u32_max / 6) {
            return err(geometry_error{
                geometry_error_code::capacity_exceeded,
                0,
            });
        }

        GeometryConfig config{};
        const u64 vertex_count = segment_count * 4;
        const u64 index_count = segment_count * 6;
        if (!config.vertices.dyarr_init_len(
                &allocator,
                vertex_count,
                vertex_count) ||
            !config.indices.dyarr_init_len(
                &allocator,
                index_count,
                index_count)) {
            return err(geometry_error{geometry_error_code::out_of_memory, 0});
        }

        const f32 segment_width = width / static_cast<f32>(x_segment_count);
        const f32 segment_height = height / static_cast<f32>(y_segment_count);
        const f32 half_width = width * 0.5f;
        const f32 half_height = height * 0.5f;
        for (u32 y = 0; y < y_segment_count; ++y) {
            for (u32 x = 0; x < x_segment_count; ++x) {
                const f32 min_x = x * segment_width - half_width;
                const f32 min_y = y * segment_height - half_height;
                const f32 max_x = min_x + segment_width;
                const f32 max_y = min_y + segment_height;
                const f32 min_u =
                    x / static_cast<f32>(x_segment_count) * tile_x;
                const f32 min_v =
                    y / static_cast<f32>(y_segment_count) * tile_y;
                const f32 max_u =
                    (x + 1) / static_cast<f32>(x_segment_count) * tile_x;
                const f32 max_v =
                    (y + 1) / static_cast<f32>(y_segment_count) * tile_y;

                const u32 vertex_offset =
                    (y * x_segment_count + x) * 4;
                config.vertices[vertex_offset] = {
                    .position = {min_x, min_y, 0.0f},
                    .normal = {},
                    .texcoord = {min_u, min_v},
                    .tangent = {},
                };
                config.vertices[vertex_offset + 1] = {
                    .position = {max_x, max_y, 0.0f},
                    .normal = {},
                    .texcoord = {max_u, max_v},
                    .tangent = {},
                };
                config.vertices[vertex_offset + 2] = {
                    .position = {min_x, max_y, 0.0f},
                    .normal = {},
                    .texcoord = {min_u, max_v},
                    .tangent = {},
                };
                config.vertices[vertex_offset + 3] = {
                    .position = {max_x, min_y, 0.0f},
                    .normal = {},
                    .texcoord = {max_u, min_v},
                    .tangent = {},
                };

                const u32 index_offset =
                    (y * x_segment_count + x) * 6;
                config.indices[index_offset] = vertex_offset;
                config.indices[index_offset + 1] = vertex_offset + 1;
                config.indices[index_offset + 2] = vertex_offset + 2;
                config.indices[index_offset + 3] = vertex_offset;
                config.indices[index_offset + 4] = vertex_offset + 3;
                config.indices[index_offset + 5] = vertex_offset + 1;
            }
        }

        auto normals_generated = generate_normals(
            cl::slice<glm::Vertex3D>{config.vertices},
            cl::slice<const u32>{config.indices});
        if (!normals_generated)
            return err(normals_generated.error());
        auto tangents_generated = generate_tangents(
            allocator,
            cl::slice<glm::Vertex3D>{config.vertices},
            cl::slice<const u32>{config.indices});
        if (!tangents_generated)
            return err(tangents_generated.error());

        config.name.assign(
            name.empty() ? default_geometry_name : name);
        config.material_name.assign(
            material_name.empty() ? default_material_name : material_name);
        config.center = {0.0f, 0.0f, 0.0f};
        config.min_extents = {-half_width, -half_height, 0.0f};
        config.max_extents = {half_width, half_height, 0.0f};
        return ok(std::move(config));
    }

    result<GeometryConfig, geometry_error> GeometrySystem::generate_cube(
        mem::Allocator& allocator,
        f32 width,
        f32 height,
        f32 depth,
        f32 tile_x,
        f32 tile_y,
        const strview name,
        const strview material_name) {
        if (width == 0.0f)
            width = 1.0f;
        if (height == 0.0f)
            height = 1.0f;
        if (depth == 0.0f)
            depth = 1.0f;
        if (tile_x == 0.0f)
            tile_x = 1.0f;
        if (tile_y == 0.0f)
            tile_y = 1.0f;

        constexpr u32 face_count = 6;
        constexpr u32 vertices_per_face = 4;
        constexpr u32 indices_per_face = 6;
        constexpr u32 vertex_count = face_count * vertices_per_face;
        constexpr u32 index_count = face_count * indices_per_face;

        GeometryConfig config{};
        if (!config.vertices.dyarr_init_len(
                &allocator,
                vertex_count,
                vertex_count) ||
            !config.indices.dyarr_init_len(
                &allocator,
                index_count,
                index_count)) {
            return err(geometry_error{geometry_error_code::out_of_memory, 0});
        }

        const f32 min_x = width * -0.5f;
        const f32 min_y = height * -0.5f;
        const f32 min_z = depth * -0.5f;
        const f32 max_x = width * 0.5f;
        const f32 max_y = height * 0.5f;
        const f32 max_z = depth * 0.5f;

        const glm::vec3 positions[vertex_count]{
            // Front (+Z).
            {min_x, min_y, max_z},
            {max_x, max_y, max_z},
            {min_x, max_y, max_z},
            {max_x, min_y, max_z},
            // Back (-Z).
            {max_x, min_y, min_z},
            {min_x, max_y, min_z},
            {max_x, max_y, min_z},
            {min_x, min_y, min_z},
            // Left (-X).
            {min_x, min_y, min_z},
            {min_x, max_y, max_z},
            {min_x, max_y, min_z},
            {min_x, min_y, max_z},
            // Right (+X).
            {max_x, min_y, max_z},
            {max_x, max_y, min_z},
            {max_x, max_y, max_z},
            {max_x, min_y, min_z},
            // Bottom (-Y).
            {max_x, min_y, max_z},
            {min_x, min_y, min_z},
            {max_x, min_y, min_z},
            {min_x, min_y, max_z},
            // Top (+Y).
            {min_x, max_y, max_z},
            {max_x, max_y, min_z},
            {min_x, max_y, min_z},
            {max_x, max_y, max_z},
        };
        const glm::vec2 texcoords[vertices_per_face]{
            {0.0f, 0.0f},
            {tile_x, tile_y},
            {0.0f, tile_y},
            {tile_x, 0.0f},
        };

        for (u32 face = 0; face < face_count; ++face) {
            const u32 vertex_offset = face * vertices_per_face;
            const u32 index_offset = face * indices_per_face;
            for (u32 vertex = 0; vertex < vertices_per_face; ++vertex) {
                config.vertices[vertex_offset + vertex] = {
                    .position = positions[vertex_offset + vertex],
                    .normal = {},
                    .texcoord = texcoords[vertex],
                    .tangent = {},
                };
            }
            config.indices[index_offset] = vertex_offset;
            config.indices[index_offset + 1] = vertex_offset + 1;
            config.indices[index_offset + 2] = vertex_offset + 2;
            config.indices[index_offset + 3] = vertex_offset;
            config.indices[index_offset + 4] = vertex_offset + 3;
            config.indices[index_offset + 5] = vertex_offset + 1;
        }

        auto normals_generated = generate_normals(
            cl::slice<glm::Vertex3D>{config.vertices},
            cl::slice<const u32>{config.indices});
        if (!normals_generated)
            return err(normals_generated.error());
        auto tangents_generated = generate_tangents(
            allocator,
            cl::slice<glm::Vertex3D>{config.vertices},
            cl::slice<const u32>{config.indices});
        if (!tangents_generated)
            return err(tangents_generated.error());

        config.name.assign(
            name.empty() ? default_geometry_name : name);
        config.material_name.assign(
            material_name.empty() ? default_material_name : material_name);
        config.center = {0.0f, 0.0f, 0.0f};
        config.min_extents = {min_x, min_y, min_z};
        config.max_extents = {max_x, max_y, max_z};
        return ok(std::move(config));
    }

    result<void, geometry_error> GeometrySystem::generate_normals(
        const cl::slice<glm::Vertex3D> vertices,
        const cl::slice<const u32> indices) {
        if (vertices.empty() || indices.empty() || indices.length() % 3 != 0) {
            return err(geometry_error{
                geometry_error_code::invalid_config,
                0,
            });
        }

        // Validate first so malformed topology never leaves partially updated
        // vertex data behind.
        for (const u32 index : indices) {
            if (index >= vertices.length()) {
                return err(geometry_error{
                    geometry_error_code::invalid_config,
                    0,
                });
            }
        }

        for (glm::Vertex3D& vertex : vertices)
            vertex.normal = glm::vec3{0.0f};

        constexpr f32 minimum_normal_length_squared = 1.0e-20f;
        for (u64 index = 0; index < indices.length(); index += 3) {
            glm::Vertex3D& a = vertices[indices[index]];
            glm::Vertex3D& b = vertices[indices[index + 1]];
            glm::Vertex3D& c = vertices[indices[index + 2]];
            const glm::vec3 face_normal = glm::cross(
                b.position - a.position,
                c.position - a.position);
            const f32 length_squared = glm::dot(face_normal, face_normal);
            if (!std::isfinite(length_squared) ||
                length_squared <= minimum_normal_length_squared) {
                continue;
            }
            a.normal += face_normal;
            b.normal += face_normal;
            c.normal += face_normal;
        }

        for (glm::Vertex3D& vertex : vertices) {
            const f32 length_squared = glm::dot(vertex.normal, vertex.normal);
            if (!std::isfinite(length_squared) ||
                length_squared <= minimum_normal_length_squared) {
                vertex.normal = glm::vec3{0.0f};
                continue;
            }
            vertex.normal *= 1.0f / std::sqrt(length_squared);
        }
        return ok();
    }

    result<void, geometry_error> GeometrySystem::generate_tangents(
        mem::Allocator& allocator,
        const cl::slice<glm::Vertex3D> vertices,
        const cl::slice<const u32> indices) {
        if (vertices.empty() || indices.empty() || indices.length() % 3 != 0) {
            return err(geometry_error{
                geometry_error_code::invalid_config,
                0,
            });
        }
        for (const u32 index : indices) {
            if (index >= vertices.length()) {
                return err(geometry_error{
                    geometry_error_code::invalid_config,
                    0,
                });
            }
        }

        cl::dyarr<glm::vec3> tangent_sums;
        cl::dyarr<glm::vec3> bitangent_sums;
        if (!tangent_sums.dyarr_init_len(
                &allocator,
                vertices.length(),
                vertices.length()) ||
            !bitangent_sums.dyarr_init_len(
                &allocator,
                vertices.length(),
                vertices.length())) {
            return err(geometry_error{geometry_error_code::out_of_memory, 0});
        }

        constexpr f32 minimum_uv_determinant = 1.0e-8f;
        for (u64 index = 0; index < indices.length(); index += 3) {
            const u32 first_index = indices[index];
            const u32 second_index = indices[index + 1];
            const u32 third_index = indices[index + 2];
            const glm::Vertex3D& first = vertices[first_index];
            const glm::Vertex3D& second = vertices[second_index];
            const glm::Vertex3D& third = vertices[third_index];

            const glm::vec3 first_edge = second.position - first.position;
            const glm::vec3 second_edge = third.position - first.position;
            const glm::vec2 first_uv_edge =
                second.texcoord - first.texcoord;
            const glm::vec2 second_uv_edge =
                third.texcoord - first.texcoord;
            const f32 determinant =
                first_uv_edge.x * second_uv_edge.y -
                second_uv_edge.x * first_uv_edge.y;
            if (!std::isfinite(determinant) ||
                std::abs(determinant) <= minimum_uv_determinant) {
                continue;
            }

            const f32 inverse_determinant = 1.0f / determinant;
            const glm::vec3 tangent =
                (first_edge * second_uv_edge.y -
                 second_edge * first_uv_edge.y) * inverse_determinant;
            const glm::vec3 bitangent =
                (second_edge * first_uv_edge.x -
                 first_edge * second_uv_edge.x) * inverse_determinant;
            const glm::vec3 safe_tangent = math::safe_normalize(tangent);
            const glm::vec3 safe_bitangent = math::safe_normalize(bitangent);
            if (safe_tangent == glm::vec3{0.0f} ||
                safe_bitangent == glm::vec3{0.0f}) {
                continue;
            }

            tangent_sums[first_index] += tangent;
            tangent_sums[second_index] += tangent;
            tangent_sums[third_index] += tangent;
            bitangent_sums[first_index] += bitangent;
            bitangent_sums[second_index] += bitangent;
            bitangent_sums[third_index] += bitangent;
        }

        for (u64 index = 0; index < vertices.length(); ++index) {
            const glm::vec3 normal = math::safe_normalize(
                vertices[index].normal);
            glm::vec3 tangent = tangent_sums[index] -
                normal * glm::dot(normal, tangent_sums[index]);
            tangent = math::safe_normalize(tangent);
            if (tangent == glm::vec3{0.0f}) {
                const glm::vec3 reference = std::abs(normal.z) < 0.999f
                    ? glm::vec3{0.0f, 0.0f, 1.0f}
                    : glm::vec3{0.0f, 1.0f, 0.0f};
                tangent = math::safe_normalize(glm::cross(reference, normal));
                if (tangent == glm::vec3{0.0f})
                    tangent = glm::vec3{1.0f, 0.0f, 0.0f};
            }

            const f32 handedness = glm::dot(
                glm::cross(normal, tangent),
                bitangent_sums[index]) < 0.0f
                ? -1.0f
                : 1.0f;
            vertices[index].tangent = glm::vec4{tangent, handedness};
        }
        return ok();
    }
}
