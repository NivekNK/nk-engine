#include <gtest/gtest.h>

#include <type_traits>

#include "memory/malloc_allocator.h"
#include "renderer/renderer.h"
#include "renderer/vulkan/swapchain.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "systems/texture_system.h"
#include "systems/material_system.h"
#include "systems/geometry_system.h"

namespace {
    class TestRenderer final : public nk::Renderer {
    public:
        enum class BeginMode {
            render,
            skip,
            fail,
        };

        TestRenderer(nk::mem::Allocator& allocator, BeginMode begin_mode)
            : Renderer{allocator, "test"}, m_begin_mode{begin_mode} {
            m_allocator = &allocator;
        }

        nk::u64 frame_number() const { return m_frame_number; }
        nk::u32 global_updates() const { return m_global_updates; }
        nk::u32 object_updates() const { return m_object_updates; }
        nk::u32 end_calls() const { return m_end_calls; }
        void fail_end(bool value) { m_fail_end = value; }
        void fail_texture_create(bool value) { m_fail_texture_create = value; }
        nk::u32 destroyed_textures() const { return m_destroyed_textures; }
        nk::u32 created_materials() const { return m_created_materials; }
        nk::u32 destroyed_materials() const { return m_destroyed_materials; }
        nk::u32 created_geometries() const { return m_created_geometries; }
        nk::u32 destroyed_geometries() const { return m_destroyed_geometries; }
        nk::result<void, nk::renderer_error> create_texture(
            nk::strview,
            nk::u32 width,
            nk::u32 height,
            nk::u32 channel_count,
            const nk::u8*,
            bool has_transparency,
            nk::Texture* texture) override {
            if (m_fail_texture_create) {
                return nk::err(nk::renderer_error{
                    .code = nk::renderer_error_code::texture_sampler_creation_failed,
                    .native_code = VK_ERROR_OUT_OF_DEVICE_MEMORY,
                });
            }
            *texture = {
                .width = width,
                .height = height,
                .channel_count = static_cast<nk::u8>(channel_count),
                .has_transparency = has_transparency,
                .generation = 0,
                .m_internal_data = reinterpret_cast<void*>(0x2),
            };
            return nk::ok();
        }

        void destroy_texture(nk::Texture* texture) override {
            ++m_destroyed_textures;
            *texture = {};
        }

        nk::result<void, nk::renderer_error> create_material(
            nk::Material& material) override {
            material.internal_id = m_created_materials++;
            return nk::ok();
        }

        void destroy_material(nk::Material& material) override {
            ++m_destroyed_materials;
            material.internal_id = nk::numeric::invalid_id;
        }

        nk::result<void, nk::renderer_error> create_geometry(
            nk::Geometry& geometry,
            nk::cl::slice<const glm::Vertex3D>,
            nk::cl::slice<const nk::u32>) override {
            geometry.internal_id = m_created_geometries++;
            geometry.generation = 0;
            return nk::ok();
        }

        void destroy_geometry(nk::Geometry& geometry) override {
            ++m_destroyed_geometries;
            geometry.internal_id = nk::numeric::invalid_id;
            geometry.generation = nk::numeric::invalid_id;
        }

    protected:
        nk::result<void, nk::renderer_error> init() override {
            return nk::ok();
        }

        void shutdown() override {}
        void on_resized(nk::u32, nk::u32) override {}

        nk::result<nk::frame_outcome, nk::renderer_error> begin_frame(
            nk::f64) override {
            switch (m_begin_mode) {
                case BeginMode::render:
                    return nk::ok(nk::frame_outcome::rendered);
                case BeginMode::skip:
                    return nk::ok(
                        nk::frame_outcome::skipped_swapchain_recreation);
                case BeginMode::fail:
                    return nk::err(nk::renderer_error{
                        .code = nk::renderer_error_code::fence_wait_failed,
                        .native_code = VK_ERROR_DEVICE_LOST,
                    });
            }
            std::abort();
        }

        void update_global_state(
            glm::mat4,
            glm::mat4,
            glm::vec3,
            glm::vec4,
            nk::i32) override {
            ++m_global_updates;
        }

        void draw_geometry(nk::GeometryRenderData) override {
            ++m_object_updates;
        }

        nk::result<nk::frame_outcome, nk::renderer_error> end_frame(
            nk::f64) override {
            ++m_end_calls;
            if (m_fail_end)
                return nk::err(nk::renderer_error{
                    .code = nk::renderer_error_code::queue_submit_failed,
                    .native_code = VK_ERROR_DEVICE_LOST,
                });
            return nk::ok(nk::frame_outcome::rendered);
        }

    private:
        BeginMode m_begin_mode;
        bool m_fail_end = false;
        bool m_fail_texture_create = false;
        nk::u32 m_global_updates = 0;
        nk::u32 m_object_updates = 0;
        nk::u32 m_end_calls = 0;
        nk::u32 m_destroyed_textures = 0;
        nk::u32 m_created_materials = 0;
        nk::u32 m_destroyed_materials = 0;
        nk::u32 m_created_geometries = 0;
        nk::u32 m_destroyed_geometries = 0;
    };

    class FailingAllocator final : public nk::mem::MallocAllocator {
    public:
        FailingAllocator() : MallocAllocator{nk::mem::untracked} {}

    protected:
        void* _do_allocate(nk::u64, nk::u64) noexcept override {
            return nullptr;
        }
    };

    class TestableVulkanRenderer final : public nk::VulkanRenderer {
    public:
        explicit TestableVulkanRenderer(nk::mem::Allocator& owner)
            : VulkanRenderer{owner, "test"} {}

        void use_runtime_allocator(nk::mem::Allocator* allocator) {
            m_allocator = allocator;
        }
    };
}

static_assert(std::is_trivially_copyable_v<nk::renderer_error>);
static_assert(sizeof(nk::renderer_error) == 8);

TEST(RendererResult, SeparatesSwapchainStatusesFromFailures) {
    auto ready = nk::vk::classify_swapchain_result(
        VK_SUCCESS,
        nk::renderer_error_code::swapchain_acquire_failed);
    ASSERT_TRUE(ready);
    EXPECT_EQ(*ready, nk::swapchain_outcome::ready);

    auto outdated = nk::vk::classify_swapchain_result(
        VK_ERROR_OUT_OF_DATE_KHR,
        nk::renderer_error_code::swapchain_acquire_failed);
    ASSERT_TRUE(outdated);
    EXPECT_EQ(*outdated, nk::swapchain_outcome::out_of_date);

    auto suboptimal = nk::vk::classify_swapchain_result(
        VK_SUBOPTIMAL_KHR,
        nk::renderer_error_code::swapchain_present_failed);
    ASSERT_TRUE(suboptimal);
    EXPECT_EQ(*suboptimal, nk::swapchain_outcome::suboptimal);

    auto failed = nk::vk::classify_swapchain_result(
        VK_ERROR_DEVICE_LOST,
        nk::renderer_error_code::swapchain_present_failed);
    ASSERT_FALSE(failed);
    EXPECT_EQ(
        failed.error().code,
        nk::renderer_error_code::swapchain_present_failed);
    EXPECT_EQ(failed.error().native_code, VK_ERROR_DEVICE_LOST);
}

TEST(RendererResult, SkippedFrameDoesNotRunOrAdvanceRenderWork) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::skip};

    auto frame = renderer.draw_frame({.delta_time = 1.0 / 60.0});

    ASSERT_TRUE(frame);
    EXPECT_EQ(*frame, nk::frame_outcome::skipped_swapchain_recreation);
    EXPECT_EQ(renderer.global_updates(), 0);
    EXPECT_EQ(renderer.object_updates(), 0);
    EXPECT_EQ(renderer.end_calls(), 0);
    EXPECT_EQ(renderer.frame_number(), 0);
}

TEST(RendererResult, PreservesBeginAndEndFailuresWithoutAdvancingFrame) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer begin_failure{allocator, TestRenderer::BeginMode::fail};

    auto begun = begin_failure.draw_frame({.delta_time = 1.0 / 60.0});
    ASSERT_FALSE(begun);
    EXPECT_EQ(begun.error().code, nk::renderer_error_code::fence_wait_failed);
    EXPECT_EQ(begun.error().native_code, VK_ERROR_DEVICE_LOST);
    EXPECT_EQ(begin_failure.frame_number(), 0);

    TestRenderer end_failure{allocator, TestRenderer::BeginMode::render};
    end_failure.fail_end(true);
    nk::GeometryRenderData geometry{};
    auto ended = end_failure.draw_frame({
        .delta_time = 1.0 / 60.0,
        .geometry_count = 1,
        .geometries = &geometry,
    });
    ASSERT_FALSE(ended);
    EXPECT_EQ(ended.error().code, nk::renderer_error_code::queue_submit_failed);
    EXPECT_EQ(ended.error().native_code, VK_ERROR_DEVICE_LOST);
    EXPECT_EQ(end_failure.global_updates(), 1);
    EXPECT_EQ(end_failure.object_updates(), 1);
    EXPECT_EQ(end_failure.end_calls(), 1);
    EXPECT_EQ(end_failure.frame_number(), 0);
}

TEST(RendererResult, AdvancesFrameOnlyAfterSuccessfulPresentation) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};

    auto frame = renderer.draw_frame({.delta_time = 1.0 / 60.0});

    ASSERT_TRUE(frame);
    EXPECT_EQ(*frame, nk::frame_outcome::rendered);
    EXPECT_EQ(renderer.frame_number(), 1);
}

TEST(RendererResult, TreatsNullFactoryDependenciesAsContractViolations) {
    EXPECT_DEATH(
        {
            auto created = nk::Renderer::create(nullptr, nullptr, "test");
            static_cast<void>(created);
        },
        "");
}

TEST(RendererResult, TextureAllocationFailureDoesNotPublishPartialState) {
    nk::mem::MallocAllocator owner{nk::mem::untracked};
    FailingAllocator failing;
    TestableVulkanRenderer renderer{owner};
    renderer.use_runtime_allocator(&failing);

    nk::Texture output{
        .id = 7,
        .width = 11,
        .height = 13,
        .channel_count = 4,
        .has_transparency = true,
        .generation = 17,
        .m_internal_data = reinterpret_cast<void*>(0x1),
    };
    const nk::Texture before = output;
    const nk::u8 pixel[4]{};

    auto created = renderer.create_texture(
        "test", 1, 1, 4, pixel, false, &output);

    ASSERT_FALSE(created);
    EXPECT_EQ(created.error().code, nk::renderer_error_code::out_of_memory);
    EXPECT_EQ(output.id, before.id);
    EXPECT_EQ(output.width, before.width);
    EXPECT_EQ(output.height, before.height);
    EXPECT_EQ(output.channel_count, before.channel_count);
    EXPECT_EQ(output.has_transparency, before.has_transparency);
    EXPECT_EQ(output.generation, before.generation);
    EXPECT_EQ(output.m_internal_data, before.m_internal_data);
}

TEST(TextureSystem, LoadsCachesAndAutoReleasesTextures) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto created = nk::TextureSystem::create(allocator, renderer, 4);
    ASSERT_TRUE(created);
    nk::TextureSystem* textures = *created;

    auto first = textures->acquire("cobblestone", true);
    ASSERT_TRUE(first);
    EXPECT_EQ((*first)->width, 512u);
    EXPECT_EQ((*first)->height, 512u);
    EXPECT_EQ((*first)->generation, 0u);

    auto second = textures->acquire("cobblestone", false);
    ASSERT_TRUE(second);
    EXPECT_EQ(*first, *second);
    EXPECT_EQ(textures->reference_count("cobblestone"), 2u);
    EXPECT_EQ(textures->loaded_count(), 1u);

    textures->release("cobblestone");
    EXPECT_EQ(textures->reference_count("cobblestone"), 1u);
    textures->release("cobblestone");
    EXPECT_EQ(textures->reference_count("cobblestone"), 0u);
    EXPECT_EQ(textures->loaded_count(), 0u);
    EXPECT_EQ(renderer.destroyed_textures(), 1u);

    nk::TextureSystem::destroy(allocator, textures);
    EXPECT_EQ(renderer.destroyed_textures(), 2u);
}

TEST(TextureSystem, DoesNotPublishFailedLoads) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto created = nk::TextureSystem::create(allocator, renderer, 2);
    ASSERT_TRUE(created);
    nk::TextureSystem* textures = *created;

    auto missing = textures->acquire("missing", true);
    ASSERT_FALSE(missing);
    EXPECT_EQ(
        missing.error().code,
        nk::texture_error_code::file_failed);
    EXPECT_EQ(textures->loaded_count(), 0u);
    EXPECT_EQ(renderer.destroyed_textures(), 0u);

    renderer.fail_texture_create(true);
    auto upload_failed = textures->acquire("cobblestone", true);
    ASSERT_FALSE(upload_failed);
    EXPECT_EQ(
        upload_failed.error().code,
        nk::texture_error_code::renderer_failed);
    EXPECT_EQ(textures->loaded_count(), 0u);

    nk::TextureSystem::destroy(allocator, textures);
    EXPECT_EQ(renderer.destroyed_textures(), 1u);
}

TEST(TextureSystem, EnforcesCapacityAndReusesReleasedSlots) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto created = nk::TextureSystem::create(allocator, renderer, 1);
    ASSERT_TRUE(created);
    nk::TextureSystem* textures = *created;

    ASSERT_TRUE(textures->acquire("cobblestone", true));
    auto full = textures->acquire("paving", true);
    ASSERT_FALSE(full);
    EXPECT_EQ(full.error().code, nk::texture_error_code::capacity_exceeded);

    textures->release("cobblestone");
    auto reused = textures->acquire("paving", true);
    ASSERT_TRUE(reused);
    EXPECT_EQ((*reused)->width, 480u);
    EXPECT_EQ(textures->loaded_count(), 1u);

    nk::TextureSystem::destroy(allocator, textures);
}

TEST(MaterialSystem, LoadsCachesAndAutoReleasesMaterialResources) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto textures_created =
        nk::TextureSystem::create(allocator, renderer, 4);
    ASSERT_TRUE(textures_created);
    nk::TextureSystem* textures = *textures_created;
    auto materials_created =
        nk::MaterialSystem::create(allocator, renderer, *textures, 4);
    ASSERT_TRUE(materials_created);
    nk::MaterialSystem* materials = *materials_created;

    auto first = materials->acquire("test_material");
    ASSERT_TRUE(first);
    EXPECT_EQ((*first)->diffuse_color, glm::vec4(1.0f));
    ASSERT_NE((*first)->diffuse_map.texture, nullptr);
    EXPECT_EQ((*first)->diffuse_map.texture->width, 480u);
    EXPECT_EQ(textures->reference_count("paving"), 1u);

    auto second = materials->acquire("test_material");
    ASSERT_TRUE(second);
    EXPECT_EQ(*first, *second);
    EXPECT_EQ(materials->reference_count("test_material"), 2u);
    EXPECT_EQ(textures->reference_count("paving"), 1u);

    materials->release("test_material");
    EXPECT_EQ(materials->loaded_count(), 1u);
    materials->release("test_material");
    EXPECT_EQ(materials->loaded_count(), 0u);
    EXPECT_EQ(textures->loaded_count(), 0u);
    EXPECT_EQ(renderer.destroyed_materials(), 1u);

    nk::MaterialSystem::destroy(allocator, materials);
    nk::TextureSystem::destroy(allocator, textures);
    EXPECT_EQ(renderer.destroyed_materials(), 2u);
}

TEST(MaterialSystem, RebindsDiffuseTexturesWithoutLeakingReferences) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto textures_created =
        nk::TextureSystem::create(allocator, renderer, 4);
    ASSERT_TRUE(textures_created);
    nk::TextureSystem* textures = *textures_created;
    auto materials_created =
        nk::MaterialSystem::create(allocator, renderer, *textures, 4);
    ASSERT_TRUE(materials_created);
    nk::MaterialSystem* materials = *materials_created;
    auto material = materials->acquire("test_material");
    ASSERT_TRUE(material);

    const nk::u32 initial_generation = (*material)->generation;
    ASSERT_TRUE(materials->set_diffuse_texture(*(*material), "cobblestone"));
    EXPECT_EQ((*material)->diffuse_map_name.view(), nk::strview{"cobblestone"});
    EXPECT_EQ((*material)->generation, initial_generation + 1);
    EXPECT_EQ(textures->reference_count("paving"), 0u);
    EXPECT_EQ(textures->reference_count("cobblestone"), 1u);

    ASSERT_TRUE(materials->set_diffuse_texture(*(*material), "paving2"));
    EXPECT_EQ(textures->reference_count("cobblestone"), 0u);
    EXPECT_EQ(textures->reference_count("paving2"), 1u);

    nk::MaterialSystem::destroy(allocator, materials);
    nk::TextureSystem::destroy(allocator, textures);
}

TEST(GeometrySystem, GeneratesSegmentedPlanesWithTiledCoordinates) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto plane = nk::GeometrySystem::generate_plane(
        allocator,
        10.0f,
        6.0f,
        2,
        3,
        4.0f,
        3.0f,
        "plane",
        "test_material");
    ASSERT_TRUE(plane);
    EXPECT_EQ(plane->vertices.length(), 24u);
    EXPECT_EQ(plane->indices.length(), 36u);
    EXPECT_EQ(plane->vertices[0].position, glm::vec3(-5.0f, -3.0f, 0.0f));
    EXPECT_EQ(plane->vertices[0].texcoord, glm::vec2(0.0f, 0.0f));
    EXPECT_EQ(plane->vertices[21].position, glm::vec3(5.0f, 3.0f, 0.0f));
    EXPECT_EQ(plane->vertices[21].texcoord, glm::vec2(4.0f, 3.0f));
}

TEST(GeometrySystem, OwnsGeometryAndMaterialReferencesUntilFinalRelease) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto textures_created =
        nk::TextureSystem::create(allocator, renderer, 4);
    ASSERT_TRUE(textures_created);
    nk::TextureSystem* textures = *textures_created;
    auto materials_created =
        nk::MaterialSystem::create(allocator, renderer, *textures, 4);
    ASSERT_TRUE(materials_created);
    nk::MaterialSystem* materials = *materials_created;
    auto geometries_created =
        nk::GeometrySystem::create(allocator, renderer, *materials, 2);
    ASSERT_TRUE(geometries_created);
    nk::GeometrySystem* geometries = *geometries_created;

    auto plane = nk::GeometrySystem::generate_plane(
        allocator,
        2.0f,
        2.0f,
        1,
        1,
        1.0f,
        1.0f,
        "managed",
        "test_material");
    ASSERT_TRUE(plane);
    auto geometry = geometries->acquire(*plane, true);
    ASSERT_TRUE(geometry);
    EXPECT_EQ((*geometry)->id, 0u);
    ASSERT_NE((*geometry)->material, nullptr);
    EXPECT_EQ((*geometry)->material->name.view(), nk::strview{"test_material"});
    EXPECT_EQ(materials->reference_count("test_material"), 1u);

    auto second = geometries->acquire((*geometry)->id);
    ASSERT_TRUE(second);
    EXPECT_EQ(*second, *geometry);
    EXPECT_EQ(geometries->reference_count((*geometry)->id), 2u);

    geometries->release(*geometry);
    EXPECT_EQ(geometries->loaded_count(), 1u);
    geometries->release(*geometry);
    EXPECT_EQ(geometries->loaded_count(), 0u);
    EXPECT_EQ(materials->loaded_count(), 0u);
    EXPECT_EQ(renderer.destroyed_geometries(), 1u);

    nk::GeometrySystem::destroy(allocator, geometries);
    nk::MaterialSystem::destroy(allocator, materials);
    nk::TextureSystem::destroy(allocator, textures);
    EXPECT_EQ(renderer.destroyed_geometries(), 2u);
}
