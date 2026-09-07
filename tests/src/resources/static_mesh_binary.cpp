#include <gtest/gtest.h>
#include <bit>
#include <cstring>

#include "core/hash.h"
#include "memory/malloc_allocator.h"
#include "resources/static_mesh_binary.h"

namespace {
    using namespace nk;
    class BudgetAllocator : public mem::MallocAllocator {
    public:
        explicit BudgetAllocator(u64 allowed) : MallocAllocator{mem::untracked}, budget{allowed} {}
        u64 calls = 0;
        u64 budget;
    protected:
        void* _do_allocate(u64 size, u64 alignment) noexcept override {
            if (calls++ >= budget) return nullptr;
            return MallocAllocator::_do_allocate(size, alignment);
        }
    };

    StaticMeshResource triangle(mem::Allocator& allocator) {
        StaticMeshResource mesh;
        EXPECT_TRUE(mesh.geometries.dyarr_init_len(&allocator, 1, 1));
        auto& geometry = mesh.geometries[0];
        geometry.name.assign("tri");
        geometry.material_name.assign("default");
        EXPECT_TRUE(geometry.vertices.dyarr_init_len(&allocator, 3, 3));
        EXPECT_TRUE(geometry.indices.dyarr_init_len(&allocator, 3, 3));
        geometry.vertices[1].position = {1, 0, 0};
        geometry.vertices[2].position = {0, 1, 0};
        for (u32 i = 0; i < 3; ++i) {
            geometry.vertices[i].normal = {0, 0, 1};
            geometry.vertices[i].tangent = {1, 0, 0, 1};
            geometry.indices[i] = i;
        }
        geometry.center = {.5f, .5f, 0};
        geometry.max_extents = {1, 1, 0};
        return mesh;
    }

    void put_u32(cl::dyarr<u8>& bytes, u64 offset, u32 value) {
        for (u32 i = 0; i < 4; ++i) bytes[offset + i] = static_cast<u8>(value >> (i * 8));
    }
    void put_u64(cl::dyarr<u8>& bytes, u64 offset, u64 value) {
        put_u32(bytes, offset, static_cast<u32>(value));
        put_u32(bytes, offset + 4, static_cast<u32>(value >> 32));
    }
    void checksum(cl::dyarr<u8>& bytes) {
        put_u64(bytes, 24, bytes.length());
        put_u64(bytes, 32, hash64_bytes(bytes.data() + mesh_binary::header_bytes,
            bytes.length() - mesh_binary::header_bytes));
    }
}

TEST(StaticMeshBinary, UsesAnExplicitLittleEndianWireLayout) {
    mem::MallocAllocator allocator{mem::untracked};
    auto mesh = triangle(allocator);
    auto encoded = mesh_binary::encode(allocator, mesh);
    ASSERT_TRUE(encoded);
    ASSERT_EQ(encoded->length(), 298);
    EXPECT_EQ(std::memcmp(encoded->data(), "NKMESH\r\n", 8), 0);
    constexpr u8 prefix[]{1, 0, 0, 0, 4, 3, 2, 1, 64, 0, 0, 0, 48, 0, 0, 0};
    EXPECT_EQ(std::memcmp(encoded->data() + 8, prefix, sizeof(prefix)), 0);
    EXPECT_EQ((*encoded)[40], 1); // One geometry; no padding-dependent count.
    EXPECT_EQ((*encoded)[82], 3); // Three vertices.
    EXPECT_EQ((*encoded)[86], 3); // Three indices.
    constexpr u8 one[]{0, 0, 128, 63};
    EXPECT_EQ(std::memcmp(encoded->data() + 190, one, 4), 0); // Vertex 1 position.x.
    EXPECT_EQ((*encoded)[294], 2); // Last index.
}

TEST(StaticMeshBinary, RoundTripsAllAttributesMaterialsAndDependenciesDeterministically) {
    mem::MallocAllocator allocator{mem::untracked};
    auto mesh = triangle(allocator);
    ASSERT_TRUE(mesh.materials.dyarr_init_len(&allocator, 1, 1));
    auto& material = mesh.materials[0];
    material.name.assign("default");
    material.shader_name.assign("Builtin.MaterialShader");
    material.diffuse_map_name.assign("diffuse");
    material.specular_map_name.assign("specular");
    material.normal_map_name.assign("normal");
    material.diffuse_color = {.1f, .2f, .3f, .4f};
    material.shininess = 17;
    material.auto_release = false;
    mesh.geometries[0].vertices[1].texcoord = {.25f, .5f};
    mesh.geometries[0].vertices[1].tangent.w = -1;
    mesh_binary::Dependency dependency;
    dependency.path.assign("models/tri.obj");
    dependency.present = true;
    dependency.size = 123;
    dependency.digest = 456;
    auto encoded = mesh_binary::encode(allocator, mesh, {&dependency, 1});
    ASSERT_TRUE(encoded);
    auto decoded = mesh_binary::decode(allocator, cl::slice<const u8>{*encoded});
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->mesh.geometries[0].vertices[1].texcoord, glm::vec2(.25f, .5f));
    EXPECT_EQ(decoded->mesh.geometries[0].vertices[1].tangent.w, -1);
    EXPECT_EQ(decoded->mesh.materials[0].diffuse_color, material.diffuse_color);
    EXPECT_EQ(decoded->mesh.materials[0].normal_map_name.view(), material.normal_map_name.view());
    EXPECT_FALSE(decoded->mesh.materials[0].auto_release);
    EXPECT_EQ(decoded->dependencies[0].digest, dependency.digest);
    auto reencoded = mesh_binary::encode(allocator, decoded->mesh,
        cl::slice<const mesh_binary::Dependency>{decoded->dependencies});
    ASSERT_TRUE(reencoded);
    ASSERT_EQ(encoded->length(), reencoded->length());
    EXPECT_EQ(std::memcmp(encoded->data(), reencoded->data(), encoded->length()), 0);
}

TEST(StaticMeshBinary, RejectsEveryTruncationWithoutAllocatingPayload) {
    mem::MallocAllocator allocator{mem::untracked};
    auto mesh = triangle(allocator);
    auto bytes = mesh_binary::encode(allocator, mesh);
    ASSERT_TRUE(bytes);
    BudgetAllocator blocked{0};
    for (u64 size = 0; size < bytes->length(); ++size) {
        auto read = mesh_binary::decode(blocked, {bytes->data(), size});
        EXPECT_FALSE(read) << size;
    }
    EXPECT_EQ(blocked.calls, 0);
}

TEST(StaticMeshBinary, RejectsHeadersAndUnsupportedVersionsWithoutAllocatingPayload) {
    mem::MallocAllocator allocator{mem::untracked};
    auto mesh = triangle(allocator);
    constexpr u64 offsets[]{0, 8, 12, 16, 20, 24, 40, 44, 48, 52, 56};
    for (u64 offset : offsets) {
        auto bytes = mesh_binary::encode(allocator, mesh);
        ASSERT_TRUE(bytes);
        put_u32(*bytes, offset, 0xffffffffu);
        BudgetAllocator blocked{0};
        auto read = mesh_binary::decode(blocked, cl::slice<const u8>{*bytes});
        EXPECT_FALSE(read) << offset;
        EXPECT_EQ(blocked.calls, 0) << offset;
    }
}

TEST(StaticMeshBinary, RejectsHostileCountsIndicesFloatsNamesAndBoundsEvenWithValidChecksum) {
    mem::MallocAllocator allocator{mem::untracked};
    auto mesh = triangle(allocator);
    constexpr u64 offsets[]{64, 82, 86, 90, 94, 98, 142, 154, 294};
    for (u64 offset : offsets) {
        auto bytes = mesh_binary::encode(allocator, mesh);
        ASSERT_TRUE(bytes);
        put_u32(*bytes, offset, 0xffffffffu);
        checksum(*bytes);
        BudgetAllocator blocked{0};
        auto read = mesh_binary::decode(blocked, cl::slice<const u8>{*bytes});
        EXPECT_FALSE(read) << offset;
        EXPECT_EQ(blocked.calls, 0) << offset;
    }
    auto bytes = mesh_binary::encode(allocator, mesh);
    ASSERT_TRUE(bytes);
    put_u32(*bytes, 142, std::bit_cast<u32>(2.0f)); // Finite, but outside declared bounds.
    checksum(*bytes);
    EXPECT_FALSE(mesh_binary::decode(allocator, cl::slice<const u8>{*bytes}));
}

TEST(StaticMeshBinary, RejectsChecksumFailuresTrailingBytesAndUnsafeDependencies) {
    mem::MallocAllocator allocator{mem::untracked};
    auto mesh = triangle(allocator);
    auto bytes = mesh_binary::encode(allocator, mesh);
    ASSERT_TRUE(bytes);
    (*bytes)[200] ^= 1;
    auto corrupt = mesh_binary::decode(allocator, cl::slice<const u8>{*bytes});
    ASSERT_FALSE(corrupt);
    EXPECT_EQ(corrupt.error(), mesh_binary::error::checksum_mismatch);
    (*bytes)[200] ^= 1;
    ASSERT_TRUE(bytes->dyarr_resize(bytes->length() + 1));
    checksum(*bytes);
    EXPECT_FALSE(mesh_binary::decode(allocator, cl::slice<const u8>{*bytes}));
    mesh_binary::Dependency dependency;
    dependency.path.assign("../outside.obj");
    EXPECT_FALSE(mesh_binary::encode(allocator, mesh, {&dependency, 1}));
    EXPECT_FALSE(mesh_binary::safe_relative_path("/absolute"));
    EXPECT_FALSE(mesh_binary::safe_relative_path("C:/absolute"));
    EXPECT_FALSE(mesh_binary::safe_relative_path("models/./tri.obj"));
    EXPECT_FALSE(mesh_binary::safe_relative_path("models//tri.obj"));
    EXPECT_TRUE(mesh_binary::safe_relative_path("models/subdir/with space.obj"));
}

TEST(StaticMeshBinary, RollsBackEveryPayloadAllocationFailure) {
    mem::MallocAllocator allocator{mem::untracked};
    auto mesh = triangle(allocator);
    ASSERT_TRUE(mesh.materials.dyarr_init_len(&allocator, 1, 1));
    mesh.materials[0].name.assign("default");
    mesh_binary::Dependency dependency;
    dependency.path.assign("models/tri.obj");
    auto bytes = mesh_binary::encode(allocator, mesh, {&dependency, 1});
    ASSERT_TRUE(bytes);
    for (u64 budget = 0; budget < 5; ++budget) {
        BudgetAllocator failing{budget};
        auto decoded = mesh_binary::decode(failing, cl::slice<const u8>{*bytes});
        ASSERT_FALSE(decoded) << budget;
        EXPECT_EQ(decoded.error(), mesh_binary::error::out_of_memory);
        EXPECT_EQ(failing.get_active_allocation_count(), 0);
    }
    BudgetAllocator failing{0};
    auto encoded = mesh_binary::encode(failing, mesh);
    ASSERT_FALSE(encoded);
    EXPECT_EQ(encoded.error(), mesh_binary::error::out_of_memory);
}
