#include <gtest/gtest.h>

#include <type_traits>

#include "memory/malloc_allocator.h"
#include "renderer/renderer.h"
#include "renderer/vulkan/swapchain.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "resources/shader_resource.h"
#include "systems/texture_system.h"
#include "systems/material_system.h"
#include "systems/geometry_system.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"

namespace {
    class TestRenderer final : public nk::Renderer {
    public:
        enum class BeginMode {
            render,
            skip,
            fail,
        };

        TestRenderer(nk::mem::Allocator& allocator, BeginMode begin_mode)
            : Renderer{allocator, nullptr, "test"}, m_begin_mode{begin_mode} {
            m_allocator = &allocator;
            m_world_shader = {0, 0};
            m_ui_shader = {1, 0};
        }

        nk::u64 frame_number() const { return m_frame_number; }
        nk::u32 global_updates() const {
            return m_world_global_updates + m_ui_global_updates;
        }
        nk::u32 object_updates() const {
            return m_world_object_updates + m_ui_object_updates;
        }
        nk::u32 world_global_updates() const { return m_world_global_updates; }
        nk::u32 ui_global_updates() const { return m_ui_global_updates; }
        nk::u32 world_object_updates() const { return m_world_object_updates; }
        nk::u32 ui_object_updates() const { return m_ui_object_updates; }
        nk::u32 pass_trace() const { return m_pass_trace; }
        nk::u32 end_calls() const { return m_end_calls; }
        void fail_end(bool value) { m_fail_end = value; }
        void fail_texture_create(bool value) { m_fail_texture_create = value; }
        void fail_shader_create(bool value) { m_fail_shader_create = value; }
        nk::u32 destroyed_textures() const { return m_destroyed_textures; }
        nk::u32 created_shaders() const { return m_created_shaders; }
        nk::u32 destroyed_shaders() const { return m_destroyed_shaders; }
        nk::u32 created_materials() const { return m_created_materials; }
        nk::u32 destroyed_materials() const { return m_destroyed_materials; }
        nk::u32 created_geometries() const { return m_created_geometries; }
        nk::u32 destroyed_geometries() const { return m_destroyed_geometries; }
        nk::u32 shader_trace_length() const { return m_shader_trace_length; }
        nk::u8 shader_trace(nk::u32 index) const {
            return m_shader_trace[index];
        }
        nk::result<nk::ShaderHandle, nk::renderer_error> create_shader(
            const nk::ShaderConfig&,
            nk::RenderPassKind) override {
            ++m_created_shaders;
            if (m_fail_shader_create) {
                return nk::err(nk::renderer_error{
                    nk::renderer_error_code::pipeline_creation_failed,
                    VK_ERROR_OUT_OF_DEVICE_MEMORY,
                });
            }
            return nk::ok(nk::ShaderHandle{
                static_cast<nk::u16>(m_next_shader_index++),
                0,
            });
        }

        nk::result<void, nk::renderer_error> destroy_shader(
            nk::ShaderHandle) override {
            ++m_destroyed_shaders;
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> use_shader(
            nk::ShaderHandle) override {
            append_shader_trace(1);
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> bind_shader_globals(
            nk::ShaderHandle) override {
            append_shader_trace(2);
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> bind_shader_instance(
            nk::ShaderHandle,
            nk::u32) override {
            append_shader_trace(5);
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> apply_shader_globals(
            const nk::ShaderHandle shader) override {
            append_shader_trace(4);
            if (shader == m_world_shader)
                ++m_world_global_updates;
            else
                ++m_ui_global_updates;
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> apply_shader_instance(
            nk::ShaderHandle) override {
            append_shader_trace(8);
            return nk::ok();
        }

        nk::result<nk::u32, nk::renderer_error> acquire_shader_instance(
            nk::ShaderHandle) override {
            return nk::ok(0u);
        }

        nk::result<void, nk::renderer_error> release_shader_instance(
            nk::ShaderHandle,
            nk::u32) override {
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> set_shader_sampler(
            nk::ShaderHandle,
            nk::ShaderUniformHandle,
            nk::Texture*) override {
            append_shader_trace(7);
            return nk::ok();
        }
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

        nk::result<void, nk::renderer_error> create_geometry(
            nk::Geometry& geometry,
            nk::cl::slice<const glm::Vertex2D>,
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

        void begin_render_pass(const nk::RenderPassKind pass) override {
            m_pass_trace = m_pass_trace * 10 +
                (pass == nk::RenderPassKind::world ? 1 : 3);
        }

        void end_render_pass(const nk::RenderPassKind pass) override {
            m_pass_trace = m_pass_trace * 10 +
                (pass == nk::RenderPassKind::world ? 2 : 4);
        }

        nk::result<void, nk::renderer_error> set_shader_uniform_raw(
            nk::ShaderHandle,
            const nk::ShaderUniformHandle uniform,
            nk::ShaderUniformType,
            const void*,
            nk::u32) override {
            if (uniform == nk::builtin_shader_uniform::projection ||
                uniform == nk::builtin_shader_uniform::view) {
                append_shader_trace(3);
            } else if (uniform == nk::builtin_shader_uniform::model) {
                append_shader_trace(9);
            } else {
                append_shader_trace(6);
            }
            return nk::ok();
        }

        void draw_geometry(
            const nk::RenderPassKind pass,
            nk::GeometryRenderData) override {
            if (pass == nk::RenderPassKind::world)
                ++m_world_object_updates;
            else
                ++m_ui_object_updates;
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
        void append_shader_trace(const nk::u8 value) {
            ASSERT_LT(m_shader_trace_length, 32u);
            m_shader_trace[m_shader_trace_length++] = value;
        }

        BeginMode m_begin_mode;
        bool m_fail_end = false;
        bool m_fail_texture_create = false;
        bool m_fail_shader_create = false;
        nk::u16 m_next_shader_index = 2;
        nk::u32 m_created_shaders = 0;
        nk::u32 m_destroyed_shaders = 0;
        nk::u32 m_world_global_updates = 0;
        nk::u32 m_ui_global_updates = 0;
        nk::u32 m_world_object_updates = 0;
        nk::u32 m_ui_object_updates = 0;
        nk::u32 m_pass_trace = 0;
        nk::u32 m_end_calls = 0;
        nk::u32 m_destroyed_textures = 0;
        nk::u32 m_created_materials = 0;
        nk::u32 m_destroyed_materials = 0;
        nk::u32 m_created_geometries = 0;
        nk::u32 m_destroyed_geometries = 0;
        nk::u8 m_shader_trace[32]{};
        nk::u32 m_shader_trace_length = 0;
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
            : VulkanRenderer{owner, nullptr, "test"} {}

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
    EXPECT_EQ(end_failure.global_updates(), 2);
    EXPECT_EQ(end_failure.object_updates(), 1);
    EXPECT_EQ(end_failure.end_calls(), 1);
    EXPECT_EQ(end_failure.frame_number(), 0);
}

TEST(RendererResult, RunsWorldAndUiPassesInOrder) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    nk::GeometryRenderData world_geometry{};
    nk::GeometryRenderData ui_geometry{};

    auto frame = renderer.draw_frame({
        .delta_time = 1.0 / 60.0,
        .geometry_count = 1,
        .geometries = &world_geometry,
        .ui_geometry_count = 1,
        .ui_geometries = &ui_geometry,
    });

    ASSERT_TRUE(frame);
    EXPECT_EQ(*frame, nk::frame_outcome::rendered);
    EXPECT_EQ(renderer.pass_trace(), 1234u);
    EXPECT_EQ(renderer.world_global_updates(), 1u);
    EXPECT_EQ(renderer.ui_global_updates(), 1u);
    EXPECT_EQ(renderer.world_object_updates(), 1u);
    EXPECT_EQ(renderer.ui_object_updates(), 1u);
}

TEST(RendererResult, UsesTheSameShaderProtocolForWorldAndUiGeometry) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};

    nk::Material world_material{};
    world_material.generation = 0;
    world_material.internal_id = 4;
    world_material.type = nk::MaterialType::world;
    nk::Geometry world_geometry{};
    world_geometry.material = &world_material;
    const nk::GeometryRenderData world_data{
        .model = glm::mat4{1.0f},
        .geometry = &world_geometry,
    };

    nk::Material ui_material{};
    ui_material.generation = 0;
    ui_material.internal_id = 9;
    ui_material.type = nk::MaterialType::ui;
    nk::Geometry ui_geometry{};
    ui_geometry.material = &ui_material;
    const nk::GeometryRenderData ui_data{
        .model = glm::mat4{1.0f},
        .geometry = &ui_geometry,
    };

    auto frame = renderer.draw_frame({
        .delta_time = 1.0 / 60.0,
        .geometry_count = 1,
        .geometries = &world_data,
        .ui_geometry_count = 1,
        .ui_geometries = &ui_data,
    });

    ASSERT_TRUE(frame);
    constexpr nk::u8 pass_protocol[] = {
        1, 2, 3, 3, 4, 5, 6, 7, 8, 9,
    };
    ASSERT_EQ(renderer.shader_trace_length(), 20u);
    for (nk::u32 pass = 0; pass < 2; ++pass) {
        for (nk::u32 operation = 0;
             operation < std::size(pass_protocol);
             ++operation) {
            EXPECT_EQ(
                renderer.shader_trace(
                    pass * std::size(pass_protocol) + operation),
                pass_protocol[operation]);
        }
    }
    EXPECT_EQ(renderer.pass_trace(), 1234u);
    EXPECT_EQ(renderer.world_global_updates(), 1u);
    EXPECT_EQ(renderer.ui_global_updates(), 1u);
    EXPECT_EQ(renderer.world_object_updates(), 1u);
    EXPECT_EQ(renderer.ui_object_updates(), 1u);
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
            auto created = nk::Renderer::create(
                nullptr,
                nullptr,
                nullptr,
                "test");
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

TEST(ShaderSystem, LoadsShadersAndResolvesNamesWithoutLookupAllocations) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto shaders_created = nk::ShaderSystem::create(
        allocator,
        renderer,
        *resources,
        {.max_shader_count = 4});
    ASSERT_TRUE(shaders_created);
    nk::ShaderSystem* shaders = *shaders_created;

    auto loaded = shaders->load("Builtin.MaterialShader");
    ASSERT_TRUE(loaded);
    EXPECT_EQ(shaders->loaded_count(), 1u);
    EXPECT_EQ(resources->active_resource_count(), 0u);

    auto by_name = shaders->handle(nk::strview{"Builtin.MaterialShader"});
    ASSERT_TRUE(by_name);
    EXPECT_EQ(*by_name, *loaded);
    auto projection = shaders->uniform(*loaded, "projection");
    auto diffuse_texture = shaders->uniform(*loaded, "diffuse_texture");
    auto model = shaders->uniform(*loaded, "model");
    ASSERT_TRUE(projection);
    ASSERT_TRUE(diffuse_texture);
    ASSERT_TRUE(model);

    auto projection_metadata = shaders->uniform_metadata(*loaded, *projection);
    ASSERT_TRUE(projection_metadata);
    EXPECT_EQ(projection_metadata->scope, nk::ShaderScope::global);
    EXPECT_EQ(projection_metadata->type, nk::ShaderUniformType::mat4);
    EXPECT_EQ(projection_metadata->offset, 0u);
    EXPECT_EQ(projection_metadata->size, sizeof(glm::mat4));
    auto texture_metadata =
        shaders->uniform_metadata(*loaded, *diffuse_texture);
    ASSERT_TRUE(texture_metadata);
    EXPECT_EQ(texture_metadata->scope, nk::ShaderScope::instance);
    EXPECT_EQ(texture_metadata->binding, 1u);
    EXPECT_EQ(texture_metadata->offset, 0u);
    EXPECT_EQ(texture_metadata->array_length, 1u);
    auto model_metadata = shaders->uniform_metadata(*loaded, *model);
    ASSERT_TRUE(model_metadata);
    EXPECT_EQ(model_metadata->scope, nk::ShaderScope::local);

    const nk::u64 allocations_before = allocator.get_active_allocation_count();
    const nk::u64 bytes_before = allocator.get_used_bytes();
    for (nk::u32 index = 0; index < 64; ++index) {
        EXPECT_TRUE(shaders->handle(nk::strview{"Builtin.MaterialShader"}));
        EXPECT_TRUE(shaders->uniform(*loaded, nk::strview{"projection"}));
    }
    EXPECT_EQ(allocator.get_active_allocation_count(), allocations_before);
    EXPECT_EQ(allocator.get_used_bytes(), bytes_before);

    nk::ShaderSystem::destroy(allocator, shaders);
    EXPECT_EQ(renderer.destroyed_shaders(), 1u);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(ShaderSystem, RoutesTypedScopeOperationsThroughTheCurrentShader) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto shaders_created = nk::ShaderSystem::create(
        allocator,
        renderer,
        *resources,
        {.max_shader_count = 2});
    ASSERT_TRUE(shaders_created);
    nk::ShaderSystem* shaders = *shaders_created;
    auto shader = shaders->load("Builtin.MaterialShader");
    ASSERT_TRUE(shader);

    auto projection = shaders->uniform(*shader, "projection");
    auto diffuse_color = shaders->uniform(*shader, "diffuse_color");
    auto diffuse_texture = shaders->uniform(*shader, "diffuse_texture");
    auto model = shaders->uniform(*shader, "model");
    ASSERT_TRUE(projection);
    ASSERT_TRUE(diffuse_color);
    ASSERT_TRUE(diffuse_texture);
    ASSERT_TRUE(model);

    auto no_current = shaders->bind_globals();
    ASSERT_FALSE(no_current);
    EXPECT_EQ(
        no_current.error().code,
        nk::shader_system_error_code::shader_not_found);

    ASSERT_TRUE(shaders->use("Builtin.MaterialShader"));
    ASSERT_TRUE(shaders->bind_globals());
    ASSERT_TRUE(shaders->set_uniform(*projection, glm::mat4{1.0f}));
    auto wrong_type = shaders->set_uniform(*projection, nk::f32{1.0f});
    ASSERT_FALSE(wrong_type);
    EXPECT_EQ(
        wrong_type.error().code,
        nk::shader_system_error_code::invalid_uniform);
    ASSERT_TRUE(shaders->apply_globals());
    auto instance = shaders->acquire_instance(*shader);
    ASSERT_TRUE(instance);
    ASSERT_TRUE(shaders->bind_instance(*instance));
    ASSERT_TRUE(shaders->set_uniform(*diffuse_color, glm::vec4{1.0f}));
    ASSERT_TRUE(shaders->set_sampler(*diffuse_texture, nullptr));
    ASSERT_TRUE(shaders->apply_instance());
    ASSERT_TRUE(shaders->set_uniform(*model, glm::mat4{1.0f}));
    ASSERT_TRUE(shaders->release_instance(*shader, *instance));

    constexpr nk::u8 expected_trace[]{1, 2, 3, 4, 5, 6, 7, 8, 9};
    ASSERT_EQ(renderer.shader_trace_length(), std::size(expected_trace));
    for (nk::u32 index = 0; index < std::size(expected_trace); ++index)
        EXPECT_EQ(renderer.shader_trace(index), expected_trace[index]);

    nk::ShaderSystem::destroy(allocator, shaders);
    nk::ResourceSystem::destroy(allocator, resources);
}

TEST(ShaderSystem, RejectsDuplicatesAndLimitsBeforeRendererPublication) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto shaders_created = nk::ShaderSystem::create(
        allocator,
        renderer,
        *resources,
        {.max_shader_count = 1});
    ASSERT_TRUE(shaders_created);
    nk::ShaderSystem* shaders = *shaders_created;

    auto first = shaders->load("Builtin.MaterialShader");
    ASSERT_TRUE(first);
    auto duplicate = shaders->load("Builtin.MaterialShader");
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(
        duplicate.error().code,
        nk::shader_system_error_code::duplicate_shader);
    auto full = shaders->load("Builtin.UIShader");
    ASSERT_FALSE(full);
    EXPECT_EQ(
        full.error().code,
        nk::shader_system_error_code::capacity_exceeded);
    EXPECT_EQ(renderer.created_shaders(), 1u);
    EXPECT_EQ(shaders->loaded_count(), 1u);
    EXPECT_EQ(resources->active_resource_count(), 0u);

    ASSERT_TRUE(shaders->destroy(*first));
    EXPECT_EQ(shaders->loaded_count(), 0u);
    EXPECT_FALSE(shaders->handle("Builtin.MaterialShader"));
    auto reused = shaders->load("Builtin.UIShader");
    ASSERT_TRUE(reused);
    EXPECT_NE(*reused, *first);
    EXPECT_EQ(shaders->loaded_count(), 1u);
    EXPECT_EQ(renderer.created_shaders(), 2u);
    EXPECT_EQ(renderer.destroyed_shaders(), 1u);

    nk::ShaderSystem::destroy(allocator, shaders);
    EXPECT_EQ(renderer.destroyed_shaders(), 2u);
    nk::ResourceSystem::destroy(allocator, resources);
}

TEST(ShaderSystem, RollsBackRendererAndResourceFailures) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto shaders_created = nk::ShaderSystem::create(
        allocator,
        renderer,
        *resources,
        {.max_shader_count = 2});
    ASSERT_TRUE(shaders_created);
    nk::ShaderSystem* shaders = *shaders_created;

    renderer.fail_shader_create(true);
    auto renderer_failed = shaders->load("Builtin.MaterialShader");
    ASSERT_FALSE(renderer_failed);
    EXPECT_EQ(
        renderer_failed.error().code,
        nk::shader_system_error_code::renderer_failed);
    EXPECT_EQ(
        renderer_failed.error().native_code,
        VK_ERROR_OUT_OF_DEVICE_MEMORY);
    EXPECT_EQ(shaders->loaded_count(), 0u);
    EXPECT_EQ(resources->active_resource_count(), 0u);
    EXPECT_FALSE(shaders->handle("Builtin.MaterialShader"));

    auto missing = shaders->load("Missing.Shader");
    ASSERT_FALSE(missing);
    EXPECT_EQ(
        missing.error().code,
        nk::shader_system_error_code::resource_failed);
    EXPECT_EQ(resources->active_resource_count(), 0u);

    nk::ShaderSystem::destroy(allocator, shaders);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(ShaderSystem, EnforcesUniformAndSamplerLimitsFromConfiguration) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_FIXTURE_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto shaders_created = nk::ShaderSystem::create(
        allocator,
        renderer,
        *resources,
        {
            .max_shader_count = 2,
            .max_uniform_count = 8,
            .max_global_samplers = 2,
            .max_instance_samplers = 2,
        });
    ASSERT_TRUE(shaders_created);
    nk::ShaderSystem* shaders = *shaders_created;

    auto too_many_samplers = shaders->load("Test.Array");
    ASSERT_FALSE(too_many_samplers);
    EXPECT_EQ(
        too_many_samplers.error().code,
        nk::shader_system_error_code::capacity_exceeded);
    EXPECT_EQ(renderer.created_shaders(), 0u);
    EXPECT_EQ(resources->active_resource_count(), 0u);

    auto invalid = shaders->load("Test.Invalid");
    ASSERT_FALSE(invalid);
    EXPECT_EQ(
        invalid.error().code,
        nk::shader_system_error_code::invalid_config);
    EXPECT_EQ(
        invalid.error().native_code,
        static_cast<nk::i32>(
            nk::shader_resource_parse_error::invalid_uniform));

    nk::ShaderSystem::destroy(allocator, shaders);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(TextureSystem, LoadsCachesAndAutoReleasesTextures) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator, NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto created = nk::TextureSystem::create(
        allocator, renderer, *resources, 4);
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
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(renderer.destroyed_textures(), 2u);
}

TEST(TextureSystem, DoesNotPublishFailedLoads) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator, NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto created = nk::TextureSystem::create(
        allocator, renderer, *resources, 2);
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
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(renderer.destroyed_textures(), 1u);
}

TEST(TextureSystem, EnforcesCapacityAndReusesReleasedSlots) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator, NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto created = nk::TextureSystem::create(
        allocator, renderer, *resources, 1);
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
    nk::ResourceSystem::destroy(allocator, resources);
}

TEST(MaterialSystem, LoadsCachesAndAutoReleasesMaterialResources) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator, NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto textures_created =
        nk::TextureSystem::create(allocator, renderer, *resources, 4);
    ASSERT_TRUE(textures_created);
    nk::TextureSystem* textures = *textures_created;
    auto materials_created =
        nk::MaterialSystem::create(
            allocator, renderer, *textures, *resources, 4);
    ASSERT_TRUE(materials_created);
    nk::MaterialSystem* materials = *materials_created;
    EXPECT_EQ(
        materials->default_material().type,
        nk::MaterialType::world);
    EXPECT_EQ(
        materials->default_ui_material().type,
        nk::MaterialType::ui);

    auto ui = materials->acquire("test_ui_material");
    ASSERT_TRUE(ui);
    EXPECT_EQ((*ui)->type, nk::MaterialType::ui);
    ASSERT_NE((*ui)->diffuse_map.texture, nullptr);
    EXPECT_EQ((*ui)->diffuse_map.texture->width, 512u);
    materials->release("test_ui_material");

    auto first = materials->acquire("test_material");
    ASSERT_TRUE(first);
    EXPECT_EQ((*first)->type, nk::MaterialType::world);
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
    EXPECT_EQ(renderer.destroyed_materials(), 2u);

    nk::MaterialSystem::destroy(allocator, materials);
    nk::TextureSystem::destroy(allocator, textures);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(renderer.destroyed_materials(), 4u);
}

TEST(MaterialSystem, RebindsDiffuseTexturesWithoutLeakingReferences) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    auto resources_created = nk::ResourceSystem::create(
        allocator, NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto textures_created =
        nk::TextureSystem::create(allocator, renderer, *resources, 4);
    ASSERT_TRUE(textures_created);
    nk::TextureSystem* textures = *textures_created;
    auto materials_created =
        nk::MaterialSystem::create(
            allocator, renderer, *textures, *resources, 4);
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
    nk::ResourceSystem::destroy(allocator, resources);
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
    auto resources_created = nk::ResourceSystem::create(
        allocator, NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto textures_created =
        nk::TextureSystem::create(allocator, renderer, *resources, 4);
    ASSERT_TRUE(textures_created);
    nk::TextureSystem* textures = *textures_created;
    auto materials_created =
        nk::MaterialSystem::create(
            allocator, renderer, *textures, *resources, 4);
    ASSERT_TRUE(materials_created);
    nk::MaterialSystem* materials = *materials_created;
    auto geometries_created =
        nk::GeometrySystem::create(allocator, renderer, *materials, 2);
    ASSERT_TRUE(geometries_created);
    nk::GeometrySystem* geometries = *geometries_created;
    EXPECT_EQ(
        geometries->default_geometry().material->type,
        nk::MaterialType::world);
    EXPECT_EQ(
        geometries->default_ui_geometry().material->type,
        nk::MaterialType::ui);

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

    auto reacquired_material = materials->acquire("test_material");
    ASSERT_TRUE(reacquired_material);
    EXPECT_TRUE((*reacquired_material)->valid());
    EXPECT_EQ(materials->reference_count("test_material"), 1u);
    materials->release((*reacquired_material)->name.view());
    EXPECT_EQ(materials->loaded_count(), 0u);

    nk::Geometry2DConfig ui_config{};
    ASSERT_TRUE(ui_config.vertices.dyarr_init_list(
        &allocator,
        {
            {{0.0f, 0.0f}, {0.0f, 0.0f}},
            {{64.0f, 64.0f}, {1.0f, 1.0f}},
            {{0.0f, 64.0f}, {0.0f, 1.0f}},
            {{64.0f, 0.0f}, {1.0f, 0.0f}},
        }));
    ASSERT_TRUE(ui_config.indices.dyarr_init_list(
        &allocator,
        {0u, 1u, 2u, 0u, 3u, 1u}));
    ui_config.name.assign("ui_quad");
    ui_config.material_name.assign("test_ui_material");

    auto ui_geometry = geometries->acquire(ui_config, true);
    ASSERT_TRUE(ui_geometry);
    EXPECT_EQ((*ui_geometry)->material->type, nk::MaterialType::ui);
    EXPECT_EQ(
        materials->reference_count("test_ui_material"),
        1u);
    geometries->release(*ui_geometry);
    EXPECT_EQ(materials->loaded_count(), 0u);

    ui_config.material_name.assign("test_material");
    auto mismatched = geometries->acquire(ui_config, true);
    ASSERT_FALSE(mismatched);
    EXPECT_EQ(
        mismatched.error().code,
        nk::geometry_error_code::invalid_config);
    EXPECT_EQ(materials->loaded_count(), 0u);

    nk::GeometrySystem::destroy(allocator, geometries);
    nk::MaterialSystem::destroy(allocator, materials);
    nk::TextureSystem::destroy(allocator, textures);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(renderer.destroyed_geometries(), 5u);
}
