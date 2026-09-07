#include "nkpch.h"
#include "resources/static_mesh_binary.h"

#include <bit>
#include <cmath>
#include "core/hash.h"

namespace nk::mesh_binary {
    namespace {
        constexpr u8 magic[]{'N', 'K', 'M', 'E', 'S', 'H', '\r', '\n'};
        constexpr u32 endian_marker = 0x01020304;
        static_assert(sizeof(f32) == 4 && std::numeric_limits<f32>::is_iec559);

        struct Counts { u32 geometries, materials, dependencies; };

        struct Reader {
            cl::slice<const u8> bytes;
            u64 offset = 0;
            error failure = error::none;

            bool good() const { return failure == error::none; }
            void reject(error reason) { if (good()) failure = reason; }
            bool available(u64 count) {
                if (!good()) return false;
                if (count > bytes.length() - offset) {
                    reject(error::truncated);
                    return false;
                }
                return true;
            }
            u32 integer() {
                if (!available(4)) return 0;
                const u8* data = bytes.data() + offset;
                offset += 4;
                return u32{data[0]} | (u32{data[1]} << 8) | (u32{data[2]} << 16) | (u32{data[3]} << 24);
            }
            u64 wide() {
                const u64 low = integer();
                return low | (u64{integer()} << 32);
            }
            f32 number() {
                const f32 value = std::bit_cast<f32>(integer());
                if (!std::isfinite(value)) reject(error::invalid_data);
                return value;
            }
            glm::vec3 vec3() { const f32 x = number(), y = number(), z = number(); return {x, y, z}; }
            glm::vec4 vec4() { const f32 x = number(), y = number(), z = number(), w = number(); return {x, y, z, w}; }
            strview string(u32 capacity, bool required = false) {
                const u32 count = integer();
                if (count > capacity || (required && count == 0)) {
                    reject(error::invalid_data);
                    return {};
                }
                if (!available(count)) return {};
                strview value{reinterpret_cast<const char*>(bytes.data() + offset), count};
                offset += count;
                for (char character : value)
                    if (character == '\0') reject(error::invalid_data);
                return value;
            }
        };

        struct Writer {
            u8* bytes = nullptr;
            u64 offset = 0;
            u64 capacity = max_file_bytes;
            bool valid = true;

            void raw(const void* data, u64 count) {
                if (!valid) return;
                if (count > capacity - offset) { valid = false; return; }
                if (bytes != nullptr && count != 0) std::memcpy(bytes + offset, data, count);
                offset += count;
            }
            void integer(u32 value) {
                const u8 data[]{static_cast<u8>(value), static_cast<u8>(value >> 8),
                    static_cast<u8>(value >> 16), static_cast<u8>(value >> 24)};
                raw(data, 4);
            }
            void wide(u64 value) { integer(static_cast<u32>(value)); integer(static_cast<u32>(value >> 32)); }
            void number(f32 value) { integer(std::bit_cast<u32>(value)); }
            void vec3(glm::vec3 value) { number(value.x); number(value.y); number(value.z); }
            void vec4(glm::vec4 value) { number(value.x); number(value.y); number(value.z); number(value.w); }
            void string(strview value) { integer(static_cast<u32>(value.length())); raw(value.data(), value.length()); }
        };

        bool inside(const glm::vec3 point, const glm::vec3 minimum, const glm::vec3 maximum) {
            return point.x >= minimum.x && point.y >= minimum.y && point.z >= minimum.z &&
                point.x <= maximum.x && point.y <= maximum.y && point.z <= maximum.z;
        }

        SamplerConfig read_sampler(Reader& reader) {
            const u32 min = reader.integer(), mag = reader.integer(), mip = reader.integer();
            const u32 u = reader.integer(), v = reader.integer(), w = reader.integer();
            const f32 anisotropy = reader.number();
            if (min > 1 || mag > 1 || mip > 1 || u > 3 || v > 3 || w > 3 || anisotropy < 1)
                reader.reject(error::invalid_data);
            return {static_cast<TextureFilter>(min), static_cast<TextureFilter>(mag),
                static_cast<TextureFilter>(mip), static_cast<TextureWrap>(u),
                static_cast<TextureWrap>(v), static_cast<TextureWrap>(w), anisotropy};
        }

        void write_sampler(Writer& writer, const SamplerConfig& sampler) {
            writer.integer(static_cast<u32>(sampler.min_filter));
            writer.integer(static_cast<u32>(sampler.mag_filter));
            writer.integer(static_cast<u32>(sampler.mip_filter));
            writer.integer(static_cast<u32>(sampler.wrap_u));
            writer.integer(static_cast<u32>(sampler.wrap_v));
            writer.integer(static_cast<u32>(sampler.wrap_w));
            writer.number(sampler.anisotropy);
        }

        void read_body(Reader& reader, Counts counts, mem::Allocator* allocator, Archive* output) {
            if (output != nullptr &&
                (!output->dependencies.dyarr_init_len(allocator, counts.dependencies, counts.dependencies) ||
                 !output->mesh.materials.dyarr_init_len(allocator, counts.materials, counts.materials) ||
                 !output->mesh.geometries.dyarr_init_len(allocator, counts.geometries, counts.geometries))) {
                reader.reject(error::out_of_memory);
                return;
            }
            u64 source_bytes = 0;
            strview dependency_paths[max_dependencies]{};
            for (u32 i = 0; i < counts.dependencies && reader.good(); ++i) {
                const u32 kind = reader.integer(), present = reader.integer();
                const u64 size = reader.wide(), digest = reader.wide();
                const strview path = reader.string(511, true);
                if (kind > static_cast<u32>(DependencyKind::presence) || present > 1 ||
                    !safe_relative_path(path) ||
                    ((present == 0 || kind == static_cast<u32>(DependencyKind::presence)) && (size != 0 || digest != 0)))
                    reader.reject(error::invalid_data);
                for (u32 previous = 0; previous < i; ++previous)
                    if (dependency_paths[previous] == path) reader.reject(error::invalid_data);
                dependency_paths[i] = path;
                if (size > max_file_bytes - source_bytes) reader.reject(error::limit_exceeded);
                else source_bytes += size;
                if (output != nullptr && reader.good()) {
                    Dependency& dependency = output->dependencies[i];
                    dependency.path.assign(path);
                    dependency.kind = static_cast<DependencyKind>(kind);
                    dependency.present = present != 0;
                    dependency.size = size;
                    dependency.digest = digest;
                }
            }
            for (u32 i = 0; i < counts.materials && reader.good(); ++i) {
                MaterialConfig material;
                material.name.assign(reader.string(material_name_capacity, true));
                material.shader_name.assign(reader.string(shader_name_capacity));
                material.diffuse_map_name.assign(reader.string(texture_name_capacity));
                material.specular_map_name.assign(reader.string(texture_name_capacity));
                material.normal_map_name.assign(reader.string(texture_name_capacity));
                const u32 type = reader.integer(), auto_release = reader.integer();
                if (type != static_cast<u32>(MaterialType::world) || auto_release > 1)
                    reader.reject(error::invalid_data);
                material.type = MaterialType::world;
                material.auto_release = auto_release != 0;
                material.diffuse_color = reader.vec4();
                material.shininess = reader.number();
                if (material.shininess <= 0) reader.reject(error::invalid_data);
                material.diffuse_sampler = read_sampler(reader);
                material.specular_sampler = read_sampler(reader);
                material.normal_sampler = read_sampler(reader);
                if (output != nullptr && reader.good()) output->mesh.materials[i] = material;
            }
            u64 vertices_total = 0, indices_total = 0;
            for (u32 i = 0; i < counts.geometries && reader.good(); ++i) {
                const strview name = reader.string(geometry_name_capacity, true);
                const strview material = reader.string(material_name_capacity, true);
                const u32 vertices = reader.integer(), indices = reader.integer();
                const u32 vertex_stride = reader.integer(), index_stride = reader.integer();
                const u64 data_size = reader.wide();
                const glm::vec3 center = reader.vec3(), minimum = reader.vec3(), maximum = reader.vec3();
                if (!reader.good()) return;
                if (vertices == 0 || vertices > max_vertices - vertices_total || indices > max_indices - indices_total) {
                    reader.reject(error::limit_exceeded);
                    return;
                }
                vertices_total += vertices;
                indices_total += indices;
                const u64 expected = u64{vertices} * vertex_bytes + u64{indices} * 4;
                if (vertex_stride != vertex_bytes || index_stride != 4 || data_size != expected ||
                    (indices == 0 ? vertices % 3 != 0 : indices % 3 != 0) || !inside(center, minimum, maximum)) {
                    reader.reject(error::invalid_data);
                    return;
                }
                if (!reader.available(data_size)) return;
                GeometryConfig* geometry = output ? &output->mesh.geometries[i] : nullptr;
                if (geometry != nullptr) {
                    geometry->name.assign(name);
                    geometry->material_name.assign(material);
                    geometry->center = center;
                    geometry->min_extents = minimum;
                    geometry->max_extents = maximum;
                    if (!geometry->vertices.dyarr_init_len(allocator, vertices, vertices) ||
                        !geometry->indices.dyarr_init_len(allocator, indices, indices)) {
                        reader.reject(error::out_of_memory);
                        return;
                    }
                }
                for (u32 vertex_index = 0; vertex_index < vertices && reader.good(); ++vertex_index) {
                    glm::Vertex3D vertex;
                    vertex.position = reader.vec3();
                    vertex.normal = reader.vec3();
                    const f32 u = reader.number(), v = reader.number();
                    vertex.texcoord = {u, v};
                    vertex.tangent = reader.vec4();
                    if (!inside(vertex.position, minimum, maximum)) reader.reject(error::invalid_data);
                    if (geometry != nullptr && reader.good()) geometry->vertices[vertex_index] = vertex;
                }
                for (u32 index = 0; index < indices && reader.good(); ++index) {
                    const u32 value = reader.integer();
                    if (value >= vertices) reader.reject(error::invalid_data);
                    if (geometry != nullptr && reader.good()) geometry->indices[index] = value;
                }
            }
            if (reader.offset != reader.bytes.length()) reader.reject(error::invalid_data);
        }

        result<Counts, error> validate(cl::slice<const u8> bytes) {
            if (bytes.length() > max_file_bytes) return err(error::limit_exceeded);
            if (bytes.length() < header_bytes) return err(error::truncated);
            if (std::memcmp(bytes.data(), magic, sizeof(magic)) != 0) return err(error::invalid_header);
            Reader reader{bytes, 8};
            if (reader.integer() != version) return err(error::incompatible_version);
            if (reader.integer() != endian_marker || reader.integer() != header_bytes ||
                reader.integer() != vertex_bytes || reader.wide() != bytes.length())
                return err(error::invalid_header);
            const u64 digest = reader.wide();
            const Counts counts{reader.integer(), reader.integer(), reader.integer()};
            if (reader.integer() != importer_revision) return err(error::incompatible_version);
            if (reader.wide() != 0) return err(error::invalid_header);
            if (counts.geometries == 0 || counts.geometries > max_geometries ||
                counts.materials > max_materials || counts.dependencies > max_dependencies)
                return err(error::limit_exceeded);
            if (hash64_bytes(bytes.data() + header_bytes, bytes.length() - header_bytes) != digest)
                return err(error::checksum_mismatch);
            read_body(reader, counts, nullptr, nullptr);
            if (!reader.good()) return err(reader.failure);
            return ok(counts);
        }

        void write_body(Writer& writer, const StaticMeshResource& mesh, cl::slice<const Dependency> dependencies) {
            for (const Dependency& dependency : dependencies) {
                writer.integer(static_cast<u32>(dependency.kind));
                writer.integer(dependency.present ? 1 : 0);
                writer.wide(dependency.size);
                writer.wide(dependency.digest);
                writer.string(dependency.path.view());
            }
            for (const MaterialConfig& material : mesh.materials) {
                writer.string(material.name.view());
                writer.string(material.shader_name.view());
                writer.string(material.diffuse_map_name.view());
                writer.string(material.specular_map_name.view());
                writer.string(material.normal_map_name.view());
                writer.integer(static_cast<u32>(material.type));
                writer.integer(material.auto_release ? 1 : 0);
                writer.vec4(material.diffuse_color);
                writer.number(material.shininess);
                write_sampler(writer, material.diffuse_sampler);
                write_sampler(writer, material.specular_sampler);
                write_sampler(writer, material.normal_sampler);
            }
            for (const GeometryConfig& geometry : mesh.geometries) {
                writer.string(geometry.name.view());
                writer.string(geometry.material_name.view());
                writer.integer(static_cast<u32>(geometry.vertices.length()));
                writer.integer(static_cast<u32>(geometry.indices.length()));
                writer.integer(vertex_bytes);
                writer.integer(4);
                writer.wide(geometry.vertices.length() * vertex_bytes + geometry.indices.length() * 4);
                writer.vec3(geometry.center);
                writer.vec3(geometry.min_extents);
                writer.vec3(geometry.max_extents);
                for (const glm::Vertex3D& vertex : geometry.vertices) {
                    writer.vec3(vertex.position);
                    writer.vec3(vertex.normal);
                    writer.number(vertex.texcoord.x);
                    writer.number(vertex.texcoord.y);
                    writer.vec4(vertex.tangent);
                }
                for (u32 index : geometry.indices) writer.integer(index);
            }
        }
    }

    bool safe_relative_path(const strview path) noexcept {
        if (path.empty() || path[0] == '/' || path[0] == '\\') return false;
        u64 component = 0;
        for (u64 i = 0; i <= path.length(); ++i) {
            if (i != path.length() && (static_cast<u8>(path[i]) < 32 || path[i] == ':' || path[i] == '\\')) return false;
            if (i == path.length() || path[i] == '/') {
                const strview part = path.substr(component, i - component);
                if (part.empty() || part == "." || part == "..") return false;
                component = i + 1;
            }
        }
        return true;
    }

    result<cl::dyarr<u8>, error> encode(mem::Allocator& allocator,
        const StaticMeshResource& mesh, cl::slice<const Dependency> dependencies) {
        if (mesh.geometries.empty() || mesh.geometries.length() > max_geometries ||
            mesh.materials.length() > max_materials || dependencies.length() > max_dependencies)
            return err(error::limit_exceeded);
        u64 total_vertices = 0, total_indices = 0;
        for (const GeometryConfig& geometry : mesh.geometries) {
            if (geometry.vertices.length() > max_vertices - total_vertices ||
                geometry.indices.length() > max_indices - total_indices)
                return err(error::limit_exceeded);
            total_vertices += geometry.vertices.length();
            total_indices += geometry.indices.length();
        }
        Writer measure{nullptr, header_bytes};
        write_body(measure, mesh, dependencies);
        if (!measure.valid) return err(error::limit_exceeded);
        cl::dyarr<u8> bytes;
        if (!bytes.dyarr_init_len(&allocator, measure.offset, measure.offset)) return err(error::out_of_memory);
        Writer writer{bytes.data(), header_bytes, bytes.length()};
        write_body(writer, mesh, dependencies);
        const u64 digest = hash64_bytes(bytes.data() + header_bytes, bytes.length() - header_bytes);
        Writer header{bytes.data(), 0, header_bytes};
        header.raw(magic, sizeof(magic));
        header.integer(version);
        header.integer(endian_marker);
        header.integer(header_bytes);
        header.integer(vertex_bytes);
        header.wide(bytes.length());
        header.wide(digest);
        header.integer(static_cast<u32>(mesh.geometries.length()));
        header.integer(static_cast<u32>(mesh.materials.length()));
        header.integer(static_cast<u32>(dependencies.length()));
        header.integer(importer_revision);
        header.wide(0);
        auto validated = validate(cl::slice<const u8>{bytes});
        if (!validated) return err(validated.error());
        return ok(std::move(bytes));
    }

    result<Archive, error> decode(mem::Allocator& allocator, cl::slice<const u8> bytes) {
        auto counts = validate(bytes);
        if (!counts) return err(counts.error());
        Archive archive;
        Reader reader{bytes, header_bytes};
        read_body(reader, *counts, &allocator, &archive);
        if (!reader.good()) return err(reader.failure);
        return ok(std::move(archive));
    }
}
