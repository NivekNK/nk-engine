#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "core/format.h"
#include "memory/malloc_allocator.h"
#include "platform/file.h"
#include "resources/loaders.h"
#include "resources/static_mesh_binary.h"

#if defined(NK_PLATFORM_LINUX)
#include <fcntl.h>
#include <unistd.h>

namespace {
    using namespace nk;
    const strview triangle = "mtllib tri.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl paint\nf 1 2 3\n";
    const strview material = "newmtl paint\nKd 0.2 0.4 0.8\nNs 16\nmap_Kd paint.png\n";

    class StaticMeshCache : public testing::Test {
    protected:
        mem::MallocAllocator allocator{mem::untracked};
        StaticMeshResourceLoader loader;
        Resource resource;
        char root[64] = "/tmp/nk-engine-mesh-XXXXXX";
        strbuf<255> previous_mode;
        bool had_mode = false;
        bool created = false;

        strbuf<511> path(strview relative) const {
            strbuf<511> output;
            EXPECT_TRUE(format_to(output, "{}/{}", strview{static_cast<cstr>(root)}, relative));
            return output;
        }
        bool write(strview relative, strview contents) {
            return static_cast<bool>(File::write_atomic(path(relative).view(),
                {reinterpret_cast<const u8*>(contents.data()), contents.length()}));
        }
        void remove(strview relative) { EXPECT_EQ(unlink(path(relative).cstr()), 0); }
        result<void, resource_error> load(strview name = "tri") {
            loader.unload(allocator, resource);
            return loader.load(allocator, strview{static_cast<cstr>(root)}, name, resource);
        }
        StaticMeshResource& mesh() { return *resource.as<StaticMeshResource>(); }
        bool binary() const { return resource.full_path.view().ends_with(".nkmesh"); }
        result<cl::dyarr<u8>, file_error> bytes() {
            File file{allocator};
            auto opened = file.open(path("models/tri.nkmesh").view(), FileMode::Read, true);
            if (!opened) return err(opened.error());
            return file.read_all_bytes(mesh_binary::max_file_bytes);
        }
        void SetUp() override {
            const char* mode = std::getenv("NK_MESH_CACHE");
            had_mode = mode != nullptr;
            if (had_mode) { ASSERT_TRUE(previous_mode.assign(strview{mode})); }
            ASSERT_EQ(setenv("NK_MESH_CACHE", "auto", 1), 0);
            ASSERT_NE(mkdtemp(root), nullptr);
            created = true;
            ASSERT_EQ(mkdir(path("models").cstr(), 0700), 0);
            ASSERT_EQ(mkdir(path("textures").cstr(), 0700), 0);
            ASSERT_TRUE(write("models/tri.obj", triangle));
            ASSERT_TRUE(write("models/tri.mtl", material));
        }
        void TearDown() override {
            loader.unload(allocator, resource);
            EXPECT_EQ(allocator.get_active_allocation_count(), 0);
            if (had_mode) setenv("NK_MESH_CACHE", previous_mode.cstr(), 1);
            else unsetenv("NK_MESH_CACHE");
            if (!created) return;
            // Only fixture-owned, enumerated files/directories are removed.
            for (strview file : {"models/tri.obj", "models/tri.mtl", "models/tri.nkmesh",
                    "textures/paint.png", "models/sub/tri.obj", "models/sub/tri.nkmesh",
                    "models/with space.mtl", "models/bad.obj"}) unlink(path(file).cstr());
            rmdir(path("models/tri.nkmesh").cstr());
            rmdir(path("models/sub").cstr());
            rmdir(path("models").cstr());
            rmdir(path("textures").cstr());
            EXPECT_EQ(rmdir(root), 0);
        }
    };
}

TEST_F(StaticMeshCache, ImportsOnMissThenLoadsEquivalentBinaryAndWritesDeterministicBytes) {
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    ASSERT_EQ(mesh().geometries.length(), 1);
    ASSERT_EQ(mesh().materials.length(), 1);
    const auto original_vertex = mesh().geometries[0].vertices[1];
    auto original = bytes();
    ASSERT_TRUE(original);
    ASSERT_TRUE(load());
    EXPECT_TRUE(binary());
    EXPECT_EQ(mesh().geometries[0].vertices[1].position, original_vertex.position);
    EXPECT_EQ(mesh().geometries[0].vertices[1].normal, original_vertex.normal);
    EXPECT_EQ(mesh().geometries[0].vertices[1].tangent, original_vertex.tangent);
    EXPECT_EQ(mesh().materials[0].diffuse_color, glm::vec4(.2f, .4f, .8f, 1));
    EXPECT_FLOAT_EQ(mesh().materials[0].shininess, 16);
    remove("models/tri.nkmesh");
    ASSERT_TRUE(load());
    auto regenerated = bytes();
    ASSERT_TRUE(regenerated);
    ASSERT_EQ(original->length(), regenerated->length());
    EXPECT_EQ(std::memcmp(original->data(), regenerated->data(), original->length()), 0);
}

TEST_F(StaticMeshCache, DetectsSameSizeSourceEditsEvenWithPreservedTimestamp) {
    ASSERT_TRUE(load());
    struct stat before{};
    ASSERT_EQ(stat(path("models/tri.obj").cstr(), &before), 0);
    ASSERT_TRUE(write("models/tri.obj", "mtllib tri.mtl\nv 0 0 0\nv 2 0 0\nv 0 1 0\nusemtl paint\nf 1 2 3\n"));
    const timespec times[]{before.st_atim, before.st_mtim};
    ASSERT_EQ(utimensat(AT_FDCWD, path("models/tri.obj").cstr(), times, 0), 0);
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    EXPECT_EQ(mesh().geometries[0].max_extents.x, 2);
    ASSERT_TRUE(load());
    EXPECT_TRUE(binary());
    EXPECT_EQ(mesh().geometries[0].max_extents.x, 2);
}

TEST_F(StaticMeshCache, InvalidatesChangedOrNewlyAvailableMaterialLibraries) {
    remove("models/tri.mtl");
    ASSERT_TRUE(load());
    EXPECT_TRUE(mesh().materials.empty());
    ASSERT_TRUE(load());
    EXPECT_TRUE(binary());
    ASSERT_TRUE(write("models/tri.mtl", material));
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    ASSERT_EQ(mesh().materials.length(), 1);
    EXPECT_FLOAT_EQ(mesh().materials[0].shininess, 16);
    ASSERT_TRUE(write("models/tri.mtl", "newmtl paint\nKd 0.2 0.4 0.8\nNs 32\nmap_Kd paint.png\n"));
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    EXPECT_FLOAT_EQ(mesh().materials[0].shininess, 32);
    ASSERT_TRUE(load());
    EXPECT_TRUE(binary());
    remove("models/tri.mtl");
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    EXPECT_TRUE(mesh().materials.empty());
}

TEST_F(StaticMeshCache, TracksTexturePresenceWithoutCachingTexturePixels) {
    ASSERT_TRUE(load());
    EXPECT_TRUE(mesh().materials[0].diffuse_map_name.empty());
    ASSERT_TRUE(write("textures/paint.png", "presence-only fixture"));
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    EXPECT_EQ(mesh().materials[0].diffuse_map_name.view(), strview{"paint"});
    ASSERT_TRUE(write("textures/paint.png", "different pixels are loaded separately"));
    ASSERT_TRUE(load());
    EXPECT_TRUE(binary());
    remove("textures/paint.png");
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    EXPECT_TRUE(mesh().materials[0].diffuse_map_name.empty());
}

TEST_F(StaticMeshCache, RegeneratesCorruptOrIncompatibleCachesAndRejectsCorruptBinaryOnlyAssets) {
    ASSERT_TRUE(load());
    for (u64 offset : {0u, 8u, 52u, 100u}) {
        auto contents = bytes();
        ASSERT_TRUE(contents);
        (*contents)[offset] ^= 0xff;
        ASSERT_TRUE(File::write_atomic(path("models/tri.nkmesh").view(), cl::slice<const u8>{*contents}));
        ASSERT_TRUE(load());
        EXPECT_FALSE(binary());
        ASSERT_TRUE(load());
        EXPECT_TRUE(binary());
    }
    ASSERT_TRUE(write("models/tri.nkmesh", "NKMESH\r\n"));
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    remove("models/tri.obj");
    remove("models/tri.mtl");
    ASSERT_TRUE(load());
    EXPECT_TRUE(binary());
    EXPECT_EQ(mesh().materials[0].name.view(), strview{"paint"});
    ASSERT_TRUE(write("models/tri.nkmesh", "broken"));
    auto invalid = load();
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, resource_error_code::decode_failed);
    EXPECT_EQ(resource.data, nullptr);
    EXPECT_EQ(resource.data_size, 0);
}

TEST_F(StaticMeshCache, KeepsImportedResourceWhenAtomicCacheReplacementFails) {
    ASSERT_EQ(mkdir(path("models/tri.nkmesh").cstr(), 0700), 0);
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    EXPECT_EQ(mesh().geometries[0].indices.length(), 3);
    struct stat status{};
    ASSERT_EQ(stat(path("models/tri.nkmesh").cstr(), &status), 0);
    EXPECT_TRUE(S_ISDIR(status.st_mode));
}

TEST_F(StaticMeshCache, AllowsOptOutWithoutReadingOrOverwritingAnExistingCache) {
    ASSERT_TRUE(load());
    ASSERT_TRUE(write("models/tri.nkmesh", "leave untouched"));
    ASSERT_EQ(setenv("NK_MESH_CACHE", "off", 1), 0);
    ASSERT_TRUE(load());
    EXPECT_FALSE(binary());
    auto contents = bytes();
    ASSERT_TRUE(contents);
    EXPECT_EQ((strview{reinterpret_cast<const char*>(contents->data()), contents->length()}), strview{"leave untouched"});
    remove("models/tri.obj");
    EXPECT_FALSE(load());
}

TEST_F(StaticMeshCache, ResolvesNestedAndEscapedMaterialNamesWithoutEscapingTheAssetRoot) {
    ASSERT_EQ(mkdir(path("models/sub").cstr(), 0700), 0);
    ASSERT_TRUE(write("models/with space.mtl", material));
    ASSERT_TRUE(write("models/sub/tri.obj", "mtllib ../with\\ space.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl paint\nf 1 2 3\n"));
    ASSERT_TRUE(load("sub/tri"));
    ASSERT_EQ(mesh().materials.length(), 1);
    EXPECT_FALSE(binary());
    ASSERT_TRUE(load("sub/tri"));
    EXPECT_TRUE(binary());
    ASSERT_TRUE(write("models/bad.obj", "mtllib ../../outside.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    auto invalid = load("bad");
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, resource_error_code::file_failed);
    EXPECT_EQ(invalid.error().native_code, static_cast<i32>(file_error::invalid_path));
    EXPECT_EQ(resource.data, nullptr);
    EXPECT_FALSE(File::exists(path("models/bad.nkmesh").cstr()));
}
#endif
