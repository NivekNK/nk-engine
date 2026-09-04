#include "nkpch.h"

#include "resources/loaders.h"

#include <bit>
#include <cmath>
#include <string>

#include "collections/map.h"
#include "core/format.h"
#include "core/hash.h"
#include "memory/allocator.h"
#include "platform/file.h"
#include "resources/static_mesh_resource.h"
#include "systems/geometry_system.h"
#include "systems/material_system.h"
#include "tinyobjloader/tiny_obj_loader.h"

namespace nk {
    namespace {
        constexpr u32 max_static_mesh_geometry_count = 4096;

        struct ObjVertexKey final {
            u32 components[11]{};

            bool operator==(const ObjVertexKey&) const noexcept = default;
        };

        u64 hash64(
            const ObjVertexKey& key,
            const u64 seed = hash_seed::deterministic) noexcept {
            return hash64_bytes(key.components, sizeof(key.components), seed);
        }

        struct ObjGroupBuild final {
            GeometryConfig geometry;
            cl::map<ObjVertexKey, u32> vertex_indices;
            u64 expected_index_count = 0;
            u64 written_index_count = 0;
            i32 material_id = -1;
            bool has_complete_normals = true;
        };

        resource_error mesh_error(
            const resource_error_code code,
            const static_mesh_parse_error detail) noexcept {
            return {code, static_cast<i32>(detail)};
        }

        resource_error geometry_failure(
            const geometry_error error) noexcept {
            if (error.code == geometry_error_code::out_of_memory) {
                return {
                    resource_error_code::out_of_memory,
                    error.native_code,
                };
            }
            return mesh_error(
                resource_error_code::invalid_data,
                static_mesh_parse_error::invalid_face);
        }

        bool prepare_resource(
            Resource& resource,
            const strview asset_base_path,
            const strview name) noexcept {
            return !name.empty() &&
                   resource.name.assign(name) &&
                   static_cast<bool>(format_to(
                       resource.full_path,
                       "{}/models/{}.obj",
                       asset_base_path,
                       name));
        }

        u32 canonical_float_bits(const f32 value) noexcept {
            if (value == 0.0f)
                return 0;
            if (std::isnan(value))
                return 0x7fc00000u;
            return std::bit_cast<u32>(value);
        }

        ObjVertexKey vertex_key(
            const glm::Vertex3D& vertex,
            const bool has_normal,
            const u32 smoothing_group,
            const u32 shape_index,
            const u32 face_index) noexcept {
            return {{
                canonical_float_bits(vertex.position.x),
                canonical_float_bits(vertex.position.y),
                canonical_float_bits(vertex.position.z),
                canonical_float_bits(vertex.normal.x),
                canonical_float_bits(vertex.normal.y),
                canonical_float_bits(vertex.normal.z),
                canonical_float_bits(vertex.texcoord.x),
                canonical_float_bits(vertex.texcoord.y),
                has_normal ? 0u : smoothing_group,
                has_normal ? 0u : shape_index + 1,
                has_normal || smoothing_group != 0 ? 0u : face_index + 1,
            }};
        }

        bool finite(const glm::vec3& value) noexcept {
            return std::isfinite(value.x) &&
                   std::isfinite(value.y) &&
                   std::isfinite(value.z);
        }

        bool finite(const glm::vec2& value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        result<glm::Vertex3D, resource_error> read_vertex(
            const tinyobj::attrib_t& attributes,
            const tinyobj::index_t& index,
            bool& has_normal) noexcept {
            if (index.vertex_index < 0) {
                return err(mesh_error(
                    resource_error_code::invalid_data,
                    static_mesh_parse_error::invalid_index));
            }

            const u64 position_offset =
                static_cast<u64>(index.vertex_index) * 3;
            if (position_offset + 2 >= attributes.vertices.size()) {
                return err(mesh_error(
                    resource_error_code::invalid_data,
                    static_mesh_parse_error::invalid_index));
            }

            glm::Vertex3D vertex{};
            vertex.position = {
                attributes.vertices[position_offset],
                attributes.vertices[position_offset + 1],
                attributes.vertices[position_offset + 2],
            };
            if (!finite(vertex.position)) {
                return err(mesh_error(
                    resource_error_code::invalid_data,
                    static_mesh_parse_error::invalid_number));
            }

            has_normal = index.normal_index >= 0;
            if (has_normal) {
                const u64 normal_offset =
                    static_cast<u64>(index.normal_index) * 3;
                if (normal_offset + 2 >= attributes.normals.size()) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::invalid_index));
                }
                vertex.normal = {
                    attributes.normals[normal_offset],
                    attributes.normals[normal_offset + 1],
                    attributes.normals[normal_offset + 2],
                };
                if (!finite(vertex.normal)) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::invalid_number));
                }
            }

            if (index.texcoord_index >= 0) {
                const u64 texcoord_offset =
                    static_cast<u64>(index.texcoord_index) * 2;
                if (texcoord_offset + 1 >= attributes.texcoords.size()) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::invalid_index));
                }
                vertex.texcoord = {
                    attributes.texcoords[texcoord_offset],
                    1.0f - attributes.texcoords[texcoord_offset + 1],
                };
                if (!finite(vertex.texcoord)) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::invalid_number));
                }
            }
            return ok(vertex);
        }

        strview texture_stem(const std::string& path) noexcept {
            const strview value{path.data(), path.size()};
            u64 begin = 0;
            for (u64 index = 0; index < value.length(); ++index) {
                if (value[index] == '/' || value[index] == '\\')
                    begin = index + 1;
            }

            u64 end = value.length();
            for (u64 index = begin; index < value.length(); ++index) {
                if (value[index] == '.')
                    end = index;
            }
            return value.substr(begin, end - begin);
        }

        bool assign_available_texture(
            const strview asset_base_path,
            const std::string& source_path,
            strbuf<texture_name_capacity>& destination) noexcept {
            const strview stem = texture_stem(source_path);
            if (stem.empty())
                return true;

            strbuf<511> png_path;
            if (!format_to(
                    png_path,
                    "{}/textures/{}.png",
                    asset_base_path,
                    stem)) {
                return false;
            }
            if (!File::exists(png_path.cstr()))
                return true;
            return destination.assign(stem);
        }

        result<void, resource_error> convert_materials(
            mem::Allocator& allocator,
            const strview asset_base_path,
            const std::vector<tinyobj::material_t>& source,
            StaticMeshResource& destination) {
            if (source.empty())
                return ok();
            if (source.size() > numeric::u32_max ||
                !destination.materials.dyarr_init_len(
                    &allocator,
                    source.size(),
                    source.size())) {
                return err(resource_error{
                    resource_error_code::out_of_memory,
                    0,
                });
            }

            for (u64 index = 0; index < source.size(); ++index) {
                const tinyobj::material_t& input = source[index];
                MaterialConfig& output = destination.materials[index];
                const strview name{input.name.data(), input.name.size()};
                if (name.empty() || !output.name.assign(name) ||
                    !output.shader_name.assign("Builtin.MaterialShader") ||
                    !assign_available_texture(
                        asset_base_path,
                        input.diffuse_texname,
                        output.diffuse_map_name) ||
                    !assign_available_texture(
                        asset_base_path,
                        input.specular_texname,
                        output.specular_map_name) ||
                    !assign_available_texture(
                        asset_base_path,
                        input.bump_texname,
                        output.normal_map_name)) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::name_too_long));
                }

                output.auto_release = true;
                output.type = MaterialType::world;
                output.diffuse_color = {
                    input.diffuse[0],
                    input.diffuse[1],
                    input.diffuse[2],
                    std::isfinite(input.dissolve) ? input.dissolve : 1.0f,
                };
                if (!std::isfinite(output.diffuse_color.x) ||
                    !std::isfinite(output.diffuse_color.y) ||
                    !std::isfinite(output.diffuse_color.z)) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::invalid_number));
                }
                output.shininess =
                    std::isfinite(input.shininess) && input.shininess > 0.0f
                        ? input.shininess
                        : 8.0f;
            }
            return ok();
        }

        result<void, resource_error> count_groups(
            mem::Allocator& allocator,
            const std::vector<tinyobj::shape_t>& shapes,
            const u64 material_count,
            cl::dyarr<ObjGroupBuild>& groups,
            cl::map<u64, u32>& group_indices) {
            if (!groups.dyarr_init(&allocator, 8)) {
                return err(resource_error{
                    resource_error_code::out_of_memory,
                    0,
                });
            }
            auto initialized = group_indices.map_init(
                &allocator,
                32,
                hash_seed::deterministic);
            if (!initialized) {
                return err(resource_error{
                    resource_error_code::out_of_memory,
                    0,
                });
            }

            if (shapes.size() > numeric::u32_max) {
                return err(mesh_error(
                    resource_error_code::capacity_exceeded,
                    static_mesh_parse_error::too_many_geometries));
            }
            for (u32 shape_index = 0; shape_index < shapes.size();
                 ++shape_index) {
                const tinyobj::mesh_t& mesh = shapes[shape_index].mesh;
                u64 source_index_offset = 0;
                for (u64 face_index = 0;
                     face_index < mesh.num_face_vertices.size();
                     ++face_index) {
                    if (face_index >= numeric::u32_max) {
                        return err(mesh_error(
                            resource_error_code::capacity_exceeded,
                            static_mesh_parse_error::invalid_face));
                    }
                    const u32 vertex_count =
                        mesh.num_face_vertices[face_index];
                    if (vertex_count < 3 ||
                        source_index_offset + vertex_count >
                            mesh.indices.size()) {
                        return err(mesh_error(
                            resource_error_code::invalid_data,
                            static_mesh_parse_error::invalid_face));
                    }
                    const u64 generated_index_count =
                        static_cast<u64>(vertex_count - 2) * 3;

                    const i32 material_id =
                        face_index < mesh.material_ids.size()
                            ? mesh.material_ids[face_index]
                            : -1;
                    if (material_id < -1 ||
                        (material_id >= 0 &&
                         static_cast<u64>(material_id) >= material_count)) {
                        return err(mesh_error(
                            resource_error_code::invalid_data,
                            static_mesh_parse_error::invalid_index));
                    }
                    const u64 key = static_cast<u32>(material_id + 1);
                    u32* group_index = group_indices.find(key);
                    if (group_index == nullptr) {
                        if (groups.length() >= max_static_mesh_geometry_count) {
                            return err(mesh_error(
                                resource_error_code::capacity_exceeded,
                                static_mesh_parse_error::too_many_geometries));
                        }
                        auto appended = groups.dyarr_emplace_back();
                        if (!appended) {
                            return err(resource_error{
                                resource_error_code::out_of_memory,
                                0,
                            });
                        }
                        ObjGroupBuild& group = groups.dyarr_last();
                        group.material_id = material_id;
                        const u32 new_index =
                            static_cast<u32>(groups.length() - 1);
                        auto inserted = group_indices.insert(key, new_index);
                        if (!inserted) {
                            return err(resource_error{
                                resource_error_code::out_of_memory,
                                0,
                            });
                        }
                        group_index = group_indices.find(key);
                    }

                    ObjGroupBuild& group = groups[*group_index];
                    if (generated_index_count > numeric::u32_max ||
                        group.expected_index_count >
                            numeric::u32_max - generated_index_count) {
                        return err(mesh_error(
                            resource_error_code::capacity_exceeded,
                            static_mesh_parse_error::invalid_face));
                    }
                    group.expected_index_count += generated_index_count;
                    source_index_offset += vertex_count;
                }
                if (source_index_offset != mesh.indices.size()) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::invalid_face));
                }
            }

            if (groups.empty()) {
                return err(mesh_error(
                    resource_error_code::invalid_data,
                    static_mesh_parse_error::empty_mesh));
            }
            return ok();
        }

        result<void, resource_error> initialize_groups(
            mem::Allocator& allocator,
            cl::dyarr<ObjGroupBuild>& groups) {
            for (ObjGroupBuild& group : groups) {
                if (!group.geometry.vertices.dyarr_init(
                        &allocator,
                        group.expected_index_count) ||
                    !group.geometry.indices.dyarr_init_len(
                        &allocator,
                        group.expected_index_count,
                        group.expected_index_count)) {
                    return err(resource_error{
                        resource_error_code::out_of_memory,
                        0,
                    });
                }
                auto initialized = group.vertex_indices.map_init(
                    &allocator,
                    group.expected_index_count,
                    hash_seed::deterministic);
                if (!initialized) {
                    return err(resource_error{
                        resource_error_code::out_of_memory,
                        0,
                    });
                }
            }
            return ok();
        }

        result<void, resource_error> convert_geometry(
            const tinyobj::attrib_t& attributes,
            const std::vector<tinyobj::shape_t>& shapes,
            const cl::map<u64, u32>& group_indices,
            cl::dyarr<ObjGroupBuild>& groups) {
            for (u32 shape_index = 0; shape_index < shapes.size();
                 ++shape_index) {
                const tinyobj::mesh_t& mesh = shapes[shape_index].mesh;
                u64 source_index_offset = 0;
                for (u64 face_index = 0;
                     face_index < mesh.num_face_vertices.size();
                     ++face_index) {
                    const i32 material_id =
                        face_index < mesh.material_ids.size()
                            ? mesh.material_ids[face_index]
                            : -1;
                    const u64 group_key =
                        static_cast<u32>(material_id + 1);
                    const u32* group_index = group_indices.find(group_key);
                    if (group_index == nullptr) {
                        return err(mesh_error(
                            resource_error_code::invalid_data,
                            static_mesh_parse_error::invalid_face));
                    }
                    ObjGroupBuild& group = groups[*group_index];

                    const u32 source_vertex_count =
                        mesh.num_face_vertices[face_index];
                    const u32 smoothing_group =
                        face_index < mesh.smoothing_group_ids.size()
                            ? mesh.smoothing_group_ids[face_index]
                            : 0;
                    for (u32 triangle = 0;
                         triangle < source_vertex_count - 2;
                         ++triangle) {
                        const u32 face_vertices[]{0, triangle + 1, triangle + 2};
                        for (const u32 face_vertex : face_vertices) {
                            bool has_normal = false;
                            auto vertex = read_vertex(
                                attributes,
                                mesh.indices[
                                    source_index_offset + face_vertex],
                                has_normal);
                            if (!vertex)
                                return err(vertex.error());
                            group.has_complete_normals &= has_normal;

                            const ObjVertexKey key = vertex_key(
                                *vertex,
                                has_normal,
                                smoothing_group,
                                shape_index,
                                static_cast<u32>(face_index));
                            const u32* existing =
                                group.vertex_indices.find(key);
                            u32 vertex_index = 0;
                            if (existing != nullptr) {
                                vertex_index = *existing;
                            } else {
                                if (group.geometry.vertices.length() >=
                                    numeric::u32_max) {
                                    return err(mesh_error(
                                        resource_error_code::capacity_exceeded,
                                        static_mesh_parse_error::invalid_index));
                                }
                                vertex_index = static_cast<u32>(
                                    group.geometry.vertices.length());
                                if (!group.geometry.vertices.dyarr_push_copy(
                                        *vertex)) {
                                    return err(resource_error{
                                        resource_error_code::out_of_memory,
                                        0,
                                    });
                                }
                                auto inserted = group.vertex_indices.insert(
                                    key,
                                    vertex_index);
                                if (!inserted) {
                                    return err(resource_error{
                                        resource_error_code::out_of_memory,
                                        0,
                                    });
                                }
                            }
                            if (group.written_index_count >=
                                group.geometry.indices.length()) {
                                return err(mesh_error(
                                    resource_error_code::invalid_data,
                                    static_mesh_parse_error::invalid_face));
                            }
                            group.geometry.indices[
                                group.written_index_count++] = vertex_index;
                        }
                    }
                    source_index_offset += source_vertex_count;
                }
            }
            return ok();
        }

        result<void, resource_error> finish_groups(
            mem::Allocator& allocator,
            const strview resource_name,
            const std::vector<tinyobj::material_t>& materials,
            cl::dyarr<ObjGroupBuild>& groups,
            StaticMeshResource& resource) {
            if (!resource.geometries.dyarr_init_len(
                    &allocator,
                    groups.length(),
                    groups.length())) {
                return err(resource_error{
                    resource_error_code::out_of_memory,
                    0,
                });
            }

            for (u64 index = 0; index < groups.length(); ++index) {
                ObjGroupBuild& group = groups[index];
                GeometryConfig& geometry = group.geometry;
                if (group.written_index_count !=
                        group.expected_index_count ||
                    geometry.vertices.empty()) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::invalid_face));
                }

                if (groups.length() == 1) {
                    if (!geometry.name.assign(resource_name)) {
                        return err(mesh_error(
                            resource_error_code::invalid_data,
                            static_mesh_parse_error::name_too_long));
                    }
                } else {
                    const std::string* material_name =
                        group.material_id >= 0
                            ? &materials[group.material_id].name
                            : nullptr;
                    const strview group_name{
                        material_name == nullptr ? "default" :
                            material_name->data(),
                        material_name == nullptr ? 7u :
                            material_name->size(),
                    };
                    if (!format_to(
                            geometry.name,
                            "{}_{}_{}",
                            resource_name,
                            group_name,
                            index)) {
                        return err(mesh_error(
                            resource_error_code::invalid_data,
                            static_mesh_parse_error::name_too_long));
                    }
                }

                if (group.material_id >= 0) {
                    const std::string& material_name =
                        materials[group.material_id].name;
                    if (!geometry.material_name.assign(strview{
                            material_name.data(),
                            material_name.size()})) {
                        return err(mesh_error(
                            resource_error_code::invalid_data,
                            static_mesh_parse_error::name_too_long));
                    }
                } else {
                    geometry.material_name.assign(default_material_name);
                }

                geometry.min_extents = geometry.vertices[0].position;
                geometry.max_extents = geometry.vertices[0].position;
                for (const glm::Vertex3D& vertex : geometry.vertices) {
                    geometry.min_extents = glm::min(
                        geometry.min_extents,
                        vertex.position);
                    geometry.max_extents = glm::max(
                        geometry.max_extents,
                        vertex.position);
                }
                geometry.center =
                    (geometry.min_extents + geometry.max_extents) * 0.5f;

                if (!group.has_complete_normals) {
                    auto generated = GeometrySystem::generate_normals(
                        cl::slice<glm::Vertex3D>{geometry.vertices},
                        cl::slice<const u32>{geometry.indices});
                    if (!generated)
                        return err(geometry_failure(generated.error()));
                }
                auto generated = GeometrySystem::generate_tangents(
                    allocator,
                    cl::slice<glm::Vertex3D>{geometry.vertices},
                    cl::slice<const u32>{geometry.indices});
                if (!generated)
                    return err(geometry_failure(generated.error()));

                resource.geometries[index] = std::move(geometry);
            }
            return ok();
        }
    }

    result<void, resource_error> StaticMeshResourceLoader::load(
        mem::Allocator& allocator,
        const strview asset_base_path,
        const strview name,
        Resource& out_resource) const {
        if (!prepare_resource(out_resource, asset_base_path, name)) {
            return err(resource_error{
                resource_error_code::invalid_name,
                0,
            });
        }

        tinyobj::ObjReaderConfig config;
        // Triangulate here so all triangles originating from one polygon keep
        // a shared flat-normal identity when the OBJ has smoothing disabled.
        config.triangulate = false;
        config.vertex_color = false;
        const std::string full_path{out_resource.full_path.cstr()};
        if (!File::exists(full_path.c_str())) {
            return err(resource_error{
                resource_error_code::file_failed,
                static_cast<i32>(file_error::not_found),
            });
        }
        const std::string::size_type separator = full_path.find_last_of("/\\");
        if (separator != std::string::npos)
            config.mtl_search_path = full_path.substr(0, separator + 1);

        tinyobj::ObjReader reader;
        if (!reader.ParseFromFile(full_path, config)) {
            if (!reader.Error().empty()) {
                ErrorLog(
                    "OBJ '{}' failed to parse: {}",
                    name,
                    strview{reader.Error().data(), reader.Error().size()});
            }
            return err(mesh_error(
                resource_error_code::decode_failed,
                static_mesh_parse_error::parser_failed));
        }
        if (!reader.Warning().empty()) {
            WarnLog(
                "OBJ '{}' parser warning: {}",
                name,
                strview{reader.Warning().data(), reader.Warning().size()});
        }

        StaticMeshResource parsed;
        auto materials_converted = convert_materials(
            allocator,
            asset_base_path,
            reader.GetMaterials(),
            parsed);
        if (!materials_converted)
            return err(materials_converted.error());

        cl::dyarr<ObjGroupBuild> groups;
        cl::map<u64, u32> group_indices;
        auto counted = count_groups(
            allocator,
            reader.GetShapes(),
            reader.GetMaterials().size(),
            groups,
            group_indices);
        if (!counted)
            return err(counted.error());

        auto initialized = initialize_groups(allocator, groups);
        if (!initialized)
            return err(initialized.error());

        auto converted = convert_geometry(
            reader.GetAttrib(),
            reader.GetShapes(),
            group_indices,
            groups);
        if (!converted)
            return err(converted.error());

        auto finished = finish_groups(
            allocator,
            name,
            reader.GetMaterials(),
            groups,
            parsed);
        if (!finished)
            return err(finished.error());

        u64 data_size = sizeof(StaticMeshResource);
        for (const GeometryConfig& geometry : parsed.geometries) {
            data_size += geometry.vertices.length() * sizeof(glm::Vertex3D);
            data_size += geometry.indices.length() * sizeof(u32);
        }
        StaticMeshResource* resource = allocator.construct_t(
            StaticMeshResource,
            std::move(parsed));
        if (resource == nullptr) {
            return err(resource_error{
                resource_error_code::out_of_memory,
                0,
            });
        }

        out_resource.data_size = data_size;
        out_resource.data = resource;
        return ok();
    }

    void StaticMeshResourceLoader::unload(
        mem::Allocator& allocator,
        Resource& resource) const noexcept {
        if (resource.data != nullptr) {
            allocator.deconstruct_t(
                StaticMeshResource,
                resource.as<StaticMeshResource>());
        }
        resource.data = nullptr;
        resource.data_size = 0;
    }
}
