#include "nkpch.h"

#include "resources/loaders.h"

#include <bit>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <istream>
#include <streambuf>
#include <string>
#include <sys/stat.h>

#include "collections/map.h"
#include "core/format.h"
#include "core/hash.h"
#include "memory/allocator.h"
#include "platform/file.h"
#include "resources/static_mesh_binary.h"
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

        resource_error file_failure(file_error failure) {
            return {failure == file_error::out_of_memory ? resource_error_code::out_of_memory :
                resource_error_code::file_failed, static_cast<i32>(failure)};
        }

        result<cl::dyarr<u8>, file_error> read_mesh_file(
            mem::Allocator& allocator, strview path, u64 max_bytes = mesh_binary::max_file_bytes) {
            strbuf<511> terminated;
            if (!terminated.assign(path)) return err(file_error::invalid_path);
            for (char character : path)
                if (character == '\0') return err(file_error::invalid_path);
            // Never open a directory, device or FIFO as an import/cache payload.
            struct stat status{};
            if (stat(terminated.cstr(), &status) != 0)
                return err(errno == ENOENT || errno == ENOTDIR ? file_error::not_found : file_error::open_failed);
#if defined(NK_PLATFORM_WINDOWS)
            if ((status.st_mode & _S_IFMT) != _S_IFREG) return err(file_error::invalid_path);
#else
            if (!S_ISREG(status.st_mode)) return err(file_error::invalid_path);
#endif
            File file{allocator};
            auto opened = file.open(path, FileMode::Read, true);
            if (!opened) return err(opened.error());
            return file.read_all_bytes(max_bytes);
        }

        // Only this non-owning stream adapter and tinyobj's own parser objects
        // cross the existing third-party STL boundary. Runtime data stays in NK containers.
        struct BorrowedInput final : std::streambuf {
            char empty = 0;
            explicit BorrowedInput(cl::dyarr<u8>& bytes) {
                char* begin = bytes.empty() ? &empty : reinterpret_cast<char*>(bytes.data());
                setg(begin, begin, begin + bytes.length());
            }
        };

        bool relative_material_path(strview directory, strview filename, strbuf<511>& output) {
            if (filename.empty() || filename[0] == '/' || filename[0] == '\\') return false;
            strbuf<1023> combined;
            if (!format_to(combined, "{}{}", directory, filename)) return false;
            output.clear();
            u64 start = 0;
            for (u64 i = 0; i <= combined.length(); ++i) {
                if (i != combined.length() && combined.data()[i] != '/' && combined.data()[i] != '\\') continue;
                const strview part = combined.view().substr(start, i - start);
                start = i + 1;
                if (part.empty() || part == ".") continue;
                if (part == "..") {
                    if (output.empty()) return false;
                    u64 end = output.length();
                    while (end != 0 && output.data()[end - 1] != '/') --end;
                    output.assign(output.view().substr(0, end == 0 ? 0 : end - 1));
                } else {
                    if (!output.empty() && !output.append('/')) return false;
                    if (!output.append(part)) return false;
                }
            }
            return mesh_binary::safe_relative_path(output.view());
        }

        struct ImportDependencies {
            mem::Allocator& allocator;
            strview base;
            cl::dyarr<mesh_binary::Dependency> entries;
            bool cacheable;

            ImportDependencies(mem::Allocator& allocator, strview base, bool enabled)
                : allocator{allocator}, base{base}, cacheable{enabled} {}

            void record(strview path, mesh_binary::DependencyKind kind, bool present,
                cl::slice<const u8> contents = {}) {
                if (!cacheable) return;
                mesh_binary::Dependency dependency;
                if (!dependency.path.assign(path) || !mesh_binary::safe_relative_path(path)) {
                    cacheable = false;
                    return;
                }
                dependency.kind = kind;
                dependency.present = present;
                if (kind == mesh_binary::DependencyKind::content && present) {
                    dependency.size = contents.length();
                    dependency.digest = hash64_bytes(contents.data(), contents.length());
                }
                for (const auto& previous : entries) {
                    if (previous.path.view() != path) continue;
                    // The source changed during this import: do not cache mixed snapshots.
                    cacheable = previous.kind == kind && previous.present == present &&
                        previous.size == dependency.size && previous.digest == dependency.digest;
                    return;
                }
                if (entries.length() == mesh_binary::max_dependencies ||
                    (entries.empty() && !entries.dyarr_init(&allocator, 8)) ||
                    !entries.dyarr_push_copy(dependency)) cacheable = false;
            }
        };

        bool dependencies_current(mem::Allocator& allocator, strview base, strview source,
            cl::slice<const mesh_binary::Dependency> dependencies) {
            bool has_source = false;
            u64 remaining = mesh_binary::max_file_bytes;
            for (const auto& dependency : dependencies) {
                strbuf<511> full_path;
                if (!format_to(full_path, "{}/{}", base, dependency.path)) return false;
                const bool present = File::exists(full_path.cstr());
                if (present != dependency.present) return false;
                if (dependency.kind == mesh_binary::DependencyKind::presence) continue;
                if (dependency.path.view() == source && present) has_source = true;
                if (!present) continue;
                auto contents = read_mesh_file(allocator, full_path.view(), remaining);
                if (!contents || contents->length() > remaining || contents->length() != dependency.size ||
                    hash64_bytes(contents->data(), contents->length()) != dependency.digest) return false;
                remaining -= contents->length();
            }
            return has_source;
        }

        struct ObjMaterialReader final : tinyobj::MaterialReader {
            ImportDependencies& dependencies;
            strview directory;
            file_error failure = file_error::none;
            u64 remaining;

            ObjMaterialReader(ImportDependencies& dependencies, strview directory, u64 obj_bytes)
                : dependencies{dependencies}, directory{directory}, remaining{mesh_binary::max_file_bytes - obj_bytes} {}

            bool operator()(const std::string& id, std::vector<tinyobj::material_t>* materials,
                std::map<std::string, int>* material_map, std::string* warning, std::string* error) override {
                if (failure != file_error::none) return false;
                strbuf<511> relative, full_path;
                if (!relative_material_path(directory, {id.data(), id.size()}, relative) ||
                    !format_to(full_path, "{}/{}", dependencies.base, relative)) {
                    failure = file_error::invalid_path;
                    return false;
                }
                auto bytes = read_mesh_file(dependencies.allocator, full_path.view(), remaining);
                if (!bytes) {
                    if (bytes.error() == file_error::not_found)
                        dependencies.record(relative.view(), mesh_binary::DependencyKind::content, false);
                    else failure = bytes.error();
                    return false;
                }
                if (bytes->length() > remaining) {
                    failure = file_error::size_limit_exceeded;
                    return false;
                }
                remaining -= bytes->length();
                dependencies.record(relative.view(), mesh_binary::DependencyKind::content, true,
                    cl::slice<const u8>{*bytes});
                BorrowedInput buffer{*bytes};
                std::istream stream{&buffer};
                tinyobj::LoadMtl(material_map, materials, &stream, warning, error);
                return true;
            }
        };

        result<void, resource_error> publish_mesh(mem::Allocator& allocator,
            StaticMeshResource&& mesh, Resource& output) {
            u64 size = sizeof(StaticMeshResource) + mesh.materials.length() * sizeof(MaterialConfig) +
                mesh.geometries.length() * sizeof(GeometryConfig);
            for (const auto& geometry : mesh.geometries)
                size += geometry.vertices.length() * sizeof(glm::Vertex3D) + geometry.indices.length() * sizeof(u32);
            auto* owned = allocator.construct_t(StaticMeshResource, std::move(mesh));
            if (owned == nullptr) return err(resource_error{resource_error_code::out_of_memory, 0});
            output.data = owned;
            output.data_size = size;
            return ok();
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
            strbuf<texture_name_capacity>& destination,
            ImportDependencies& dependencies) noexcept {
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
            const bool present = File::exists(png_path.cstr());
            strbuf<511> relative;
            if (format_to(relative, "textures/{}.png", stem))
                dependencies.record(relative.view(), mesh_binary::DependencyKind::presence, present);
            else dependencies.cacheable = false;
            if (!present)
                return true;
            return destination.assign(stem);
        }

        result<void, resource_error> convert_materials(
            mem::Allocator& allocator,
            const strview asset_base_path,
            const std::vector<tinyobj::material_t>& source,
            StaticMeshResource& destination,
            ImportDependencies& dependencies) {
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
                        output.diffuse_map_name, dependencies) ||
                    !assign_available_texture(
                        asset_base_path,
                        input.specular_texname,
                        output.specular_map_name, dependencies) ||
                    !assign_available_texture(
                        asset_base_path,
                        input.bump_texname,
                        output.normal_map_name, dependencies)) {
                    return err(mesh_error(
                        resource_error_code::invalid_data,
                        static_mesh_parse_error::name_too_long));
                }

                output.auto_release = true;
                output.type = MaterialType::world;
                output.blend_mode =
                    std::isfinite(input.dissolve) && input.dissolve < 1.0f
                        ? MaterialBlendMode::transparent
                        : MaterialBlendMode::opaque;
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
        const auto started = std::chrono::steady_clock::now();
        strbuf<511> source_relative, cache_path;
        if (!prepare_resource(out_resource, asset_base_path, name) ||
            !format_to(source_relative, "models/{}.obj", name) ||
            !mesh_binary::safe_relative_path(source_relative.view()) ||
            !format_to(cache_path, "{}/models/{}.nkmesh", asset_base_path, name)) {
            return err(resource_error{
                resource_error_code::invalid_name,
                0,
            });
        }

        const auto report_load = [&](strview route) {
            const f64 milliseconds = std::chrono::duration<f64, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            InfoLog("Static mesh '{}' loaded from {} in {:.3f} ms", name, route, milliseconds);
        };
        const char* mode = std::getenv("NK_MESH_CACHE");
        const bool cache_enabled = mode == nullptr || strview{mode} != "off";
        const bool source_present = File::exists(out_resource.full_path.cstr());
        if (cache_enabled && File::exists(cache_path.cstr())) {
            auto bytes = read_mesh_file(allocator, cache_path.view());
            if (bytes) {
                auto archive = mesh_binary::decode(allocator, cl::slice<const u8>{*bytes});
                if (archive) {
                    if (!source_present || dependencies_current(allocator, asset_base_path,
                            source_relative.view(), cl::slice<const mesh_binary::Dependency>{archive->dependencies})) {
                        auto published = publish_mesh(allocator, std::move(archive->mesh), out_resource);
                        if (!published) return published;
                        out_resource.full_path = cache_path;
                        report_load("nkmesh");
                        return ok();
                    }
                    InfoLog("Static mesh '{}' cache dependencies changed; importing OBJ", name);
                } else {
                    if (archive.error() == mesh_binary::error::out_of_memory)
                        return err(resource_error{resource_error_code::out_of_memory, 0});
                    if (!source_present)
                        return err(resource_error{resource_error_code::decode_failed, static_cast<i32>(archive.error())});
                    WarnLog("Static mesh '{}' cache rejected ({}); importing OBJ", name, static_cast<i32>(archive.error()));
                }
            } else {
                if (!source_present || bytes.error() == file_error::out_of_memory)
                    return err(file_failure(bytes.error()));
                WarnLog("Static mesh '{}' cache unreadable ({}); importing OBJ", name, static_cast<i32>(bytes.error()));
            }
        }

        auto source = read_mesh_file(allocator, out_resource.full_path.view());
        if (!source) return err(file_failure(source.error()));
        ImportDependencies dependencies{allocator, asset_base_path, cache_enabled};
        dependencies.record(source_relative.view(), mesh_binary::DependencyKind::content, true,
            cl::slice<const u8>{*source});
        u64 directory_end = source_relative.length();
        while (directory_end != 0 && source_relative.data()[directory_end - 1] != '/') --directory_end;
        ObjMaterialReader material_reader{dependencies, source_relative.view().substr(0, directory_end), source->length()};
        BorrowedInput buffer{*source};
        std::istream stream{&buffer};
        tinyobj::attrib_t attributes;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warning, error;
        // Keep NK's triangulation, normal generation and UV convention intact.
        const bool imported = tinyobj::LoadObj(&attributes, &shapes, &materials,
            &warning, &error, &stream, &material_reader, false, false);
        if (material_reader.failure != file_error::none) return err(file_failure(material_reader.failure));
        if (!imported) {
            if (!error.empty()) {
                ErrorLog(
                    "OBJ '{}' failed to parse: {}",
                    name,
                    strview{error.data(), error.size()});
            }
            return err(mesh_error(
                resource_error_code::decode_failed,
                static_mesh_parse_error::parser_failed));
        }
        if (!warning.empty()) {
            WarnLog(
                "OBJ '{}' parser warning: {}",
                name,
                strview{warning.data(), warning.size()});
        }

        StaticMeshResource parsed;
        auto materials_converted = convert_materials(
            allocator,
            asset_base_path,
            materials,
            parsed,
            dependencies);
        if (!materials_converted)
            return err(materials_converted.error());

        cl::dyarr<ObjGroupBuild> groups;
        cl::map<u64, u32> group_indices;
        auto counted = count_groups(
            allocator,
            shapes,
            materials.size(),
            groups,
            group_indices);
        if (!counted)
            return err(counted.error());

        auto initialized = initialize_groups(allocator, groups);
        if (!initialized)
            return err(initialized.error());

        auto converted = convert_geometry(
            attributes,
            shapes,
            group_indices,
            groups);
        if (!converted)
            return err(converted.error());

        auto finished = finish_groups(
            allocator,
            name,
            materials,
            groups,
            parsed);
        if (!finished)
            return err(finished.error());

        if (dependencies.cacheable) {
            auto encoded = mesh_binary::encode(allocator, parsed,
                cl::slice<const mesh_binary::Dependency>{dependencies.entries});
            if (encoded) {
                auto saved = File::write_atomic(cache_path.view(), cl::slice<const u8>{*encoded});
                if (!saved)
                    WarnLog("Static mesh '{}' cache write skipped ({}); OBJ remains usable", name, static_cast<i32>(saved.error()));
            } else {
                WarnLog("Static mesh '{}' cache encoding skipped ({}); OBJ remains usable", name, static_cast<i32>(encoded.error()));
            }
        } else if (cache_enabled) {
            WarnLog("Static mesh '{}' dependency tracking unavailable; OBJ remains usable", name);
        }
        auto published = publish_mesh(allocator, std::move(parsed), out_resource);
        if (!published) return published;
        report_load("OBJ");
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
