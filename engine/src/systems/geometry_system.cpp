#include "nkpch.h"

#include "systems/geometry_system.h"

#include "memory/allocator.h"
#include "renderer/renderer.h"
#include "systems/material_system.h"

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
            .texcoord = {0.0f, 0.0f},
        };
        vertices[1] = {
            .position = {0.5f * scale, 0.5f * scale, 0.0f},
            .texcoord = {1.0f, 1.0f},
        };
        vertices[2] = {
            .position = {-0.5f * scale, 0.5f * scale, 0.0f},
            .texcoord = {0.0f, 1.0f},
        };
        vertices[3] = {
            .position = {0.5f * scale, -0.5f * scale, 0.0f},
            .texcoord = {1.0f, 0.0f},
        };
        const u32 indices[6]{0, 1, 2, 0, 3, 1};

        Geometry world{};
        world.name.assign(default_geometry_name);
        world.material = &m_materials->default_material();
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
            auto_release);
    }

    template<typename Vertex>
    result<Geometry*, geometry_error> GeometrySystem::acquire_geometry(
        const cl::slice<const Vertex> vertices,
        const cl::slice<const u32> indices,
        const strview name,
        const strview material_name,
        const MaterialType material_type,
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
        auto created = create_geometry(
            vertices,
            indices,
            name,
            material_name,
            material_type,
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
        Geometry& geometry) {
        const strview fallback_geometry_name =
            material_type == MaterialType::world
                ? default_geometry_name
                : default_ui_geometry_name;
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
            : &m_materials->default_ui_material();
        if (material_name.empty() ||
            material_name == default_material_name ||
            material_name == default_material->name.view()) {
            geometry.material = default_material;
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
                    .texcoord = {min_u, min_v},
                };
                config.vertices[vertex_offset + 1] = {
                    .position = {max_x, max_y, 0.0f},
                    .texcoord = {max_u, max_v},
                };
                config.vertices[vertex_offset + 2] = {
                    .position = {min_x, max_y, 0.0f},
                    .texcoord = {min_u, max_v},
                };
                config.vertices[vertex_offset + 3] = {
                    .position = {max_x, min_y, 0.0f},
                    .texcoord = {max_u, min_v},
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

        config.name.assign(
            name.empty() ? default_geometry_name : name);
        config.material_name.assign(
            material_name.empty() ? default_material_name : material_name);
        return ok(std::move(config));
    }
}
