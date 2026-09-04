#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstring>

#include <type_traits>

#include <glm/gtc/matrix_transform.hpp>

#include "memory/malloc_allocator.h"
#include "core/input_codes.h"
#include "core/render_view_controls.h"
#include "renderer/renderer.h"
#include "renderer/vulkan/swapchain.h"
#include "renderer/vulkan/vulkan_renderer.h"
#include "resources/mesh.h"
#include "resources/shader_resource.h"
#include "systems/texture_system.h"
#include "systems/event_system.h"
#include "systems/material_system.h"
#include "systems/geometry_system.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"

namespace {
    static_assert(sizeof(nk::PointLightUniform) == 48);
    static_assert(alignof(nk::PointLightUniform) == 16);
    static_assert(offsetof(nk::PointLightUniform, position) == 0);
    static_assert(offsetof(nk::PointLightUniform, constant) == 12);
    static_assert(offsetof(nk::PointLightUniform, color) == 16);
    static_assert(offsetof(nk::PointLightUniform, linear) == 32);
    static_assert(offsetof(nk::PointLightUniform, quadratic) == 36);
    static_assert(offsetof(nk::PointLightUniform, padding) == 40);

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
        void fail_texture_create_on_call(nk::u32 call) {
            m_failed_texture_create_call = call;
        }
        void fail_shader_create(bool value) { m_fail_shader_create = value; }
        void fail_instance_acquire_on_call(nk::u32 call) {
            m_failed_instance_acquire_call = call;
        }
        nk::u32 destroyed_textures() const { return m_destroyed_textures; }
        nk::u32 created_textures() const { return m_created_textures; }
        bool default_specular_is_black() const {
            return m_default_specular_is_black;
        }
        bool default_normal_is_flat() const {
            return m_default_normal_is_flat;
        }
        nk::u32 created_shaders() const { return m_created_shaders; }
        nk::u32 destroyed_shaders() const { return m_destroyed_shaders; }
        nk::u32 sampler_array_index() const { return m_sampler_array_index; }
        nk::u32 acquired_instances() const { return m_acquired_instances; }
        nk::u32 released_instances() const { return m_released_instances; }
        nk::u32 instance_apply_count() const { return m_instance_apply_count; }
        nk::u32 instance_update_count() const {
            return m_instance_update_count;
        }
        nk::u32 created_geometries() const { return m_created_geometries; }
        nk::u32 destroyed_geometries() const { return m_destroyed_geometries; }
        nk::u32 shader_trace_length() const { return m_shader_trace_length; }
        nk::u8 shader_trace(nk::u32 index) const {
            return m_shader_trace[index];
        }
        const glm::vec4& ambient_color() const { return m_ambient_color; }
        const glm::vec3& directional_light_direction() const {
            return m_directional_light_direction;
        }
        const glm::vec4& directional_light_color() const {
            return m_directional_light_color;
        }
        const glm::vec3& view_position() const { return m_view_position_value; }
        nk::u32 point_light_count() const { return m_point_light_count; }
        const nk::PointLightUniform& point_light(const nk::u32 index) const {
            return m_point_lights[index];
        }
        nk::u32 uploaded_render_view_mode() const {
            return m_uploaded_render_view_mode;
        }
        nk::Texture* specular_sampler_texture() const {
            return m_specular_sampler_texture;
        }
        nk::Texture* normal_sampler_texture() const {
            return m_normal_sampler_texture;
        }
        nk::f32 shininess() const { return m_shininess; }
        nk::result<nk::ShaderHandle, nk::renderer_error> create_shader(
            const nk::ShaderConfig&,
            const nk::RenderPassKind pass) override {
            ++m_created_shaders;
            if (m_fail_shader_create) {
                return nk::err(nk::renderer_error{
                    nk::renderer_error_code::pipeline_creation_failed,
                    VK_ERROR_OUT_OF_DEVICE_MEMORY,
                });
            }
            const nk::ShaderHandle handle{
                static_cast<nk::u16>(m_next_shader_index++),
                0,
            };
            if (pass == nk::RenderPassKind::world)
                m_world_test_shader = handle;
            else
                m_ui_test_shader = handle;
            return nk::ok(handle);
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
            if (shader == m_world_test_shader)
                ++m_world_global_updates;
            else
                ++m_ui_global_updates;
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> apply_shader_instance(
            nk::ShaderHandle,
            const bool needs_update) override {
            ++m_instance_apply_count;
            if (needs_update)
                ++m_instance_update_count;
            append_shader_trace(8);
            return nk::ok();
        }

        nk::result<nk::u32, nk::renderer_error> acquire_shader_instance(
            const nk::ShaderHandle shader) override {
            ++m_instance_acquire_attempts;
            if (m_instance_acquire_attempts == m_failed_instance_acquire_call) {
                return nk::err(nk::renderer_error{
                    nk::renderer_error_code::out_of_memory,
                    VK_ERROR_OUT_OF_DEVICE_MEMORY,
                });
            }
            ++m_acquired_instances;
            return nk::ok(m_next_instance_id[shader.index]++);
        }

        nk::result<void, nk::renderer_error> release_shader_instance(
            nk::ShaderHandle,
            nk::u32) override {
            ++m_released_instances;
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> set_shader_sampler(
            nk::ShaderHandle,
            const nk::ShaderUniformHandle uniform,
            nk::Texture* texture,
            const nk::u32 array_index) override {
            m_sampler_array_index = array_index;
            if (uniform == nk::builtin_shader_uniform::specular_texture)
                m_specular_sampler_texture = texture;
            if (uniform == nk::builtin_shader_uniform::normal_texture)
                m_normal_sampler_texture = texture;
            append_shader_trace(7);
            return nk::ok();
        }
        nk::result<void, nk::renderer_error> create_texture(
            const nk::strview name,
            nk::u32 width,
            nk::u32 height,
            nk::u32 channel_count,
            const nk::u8* pixels,
            bool has_transparency,
            nk::Texture* texture) override {
            ++m_texture_create_attempts;
            if (m_fail_texture_create ||
                m_texture_create_attempts == m_failed_texture_create_call) {
                return nk::err(nk::renderer_error{
                    .code = nk::renderer_error_code::texture_sampler_creation_failed,
                    .native_code = VK_ERROR_OUT_OF_DEVICE_MEMORY,
                });
            }
            ++m_created_textures;
            if (name == nk::default_specular_texture_name) {
                m_default_specular_is_black =
                    width == 1 && height == 1 && channel_count == 4 &&
                    pixels != nullptr && pixels[0] == 0 && pixels[1] == 0 &&
                    pixels[2] == 0 && pixels[3] == 255;
            }
            if (name == nk::default_normal_texture_name) {
                m_default_normal_is_flat =
                    width == 1 && height == 1 && channel_count == 4 &&
                    pixels != nullptr && pixels[0] == 128 &&
                    pixels[1] == 128 && pixels[2] == 255 &&
                    pixels[3] == 255;
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
            const nk::ShaderHandle shader,
            const nk::ShaderUniformHandle uniform,
            nk::ShaderUniformType,
            const void* data,
            const nk::u32 size) override {
            if (shader == m_world_test_shader && data != nullptr) {
                if (uniform == nk::builtin_shader_uniform::ambient_color &&
                    size == sizeof(glm::vec4)) {
                    m_ambient_color = *static_cast<const glm::vec4*>(data);
                } else if (
                    uniform == nk::builtin_shader_uniform::directional_light_direction &&
                    size == sizeof(glm::vec3)) {
                    m_directional_light_direction =
                        *static_cast<const glm::vec3*>(data);
                } else if (
                    uniform == nk::builtin_shader_uniform::directional_light_color &&
                    size == sizeof(glm::vec4)) {
                    m_directional_light_color =
                        *static_cast<const glm::vec4*>(data);
                } else if (
                    uniform == nk::builtin_shader_uniform::shininess &&
                    size == sizeof(nk::f32)) {
                    m_shininess = *static_cast<const nk::f32*>(data);
                } else if (
                    uniform == nk::builtin_shader_uniform::view_position &&
                    size == sizeof(glm::vec3)) {
                    m_view_position_value =
                        *static_cast<const glm::vec3*>(data);
                } else if (
                    uniform == nk::builtin_shader_uniform::point_light_count &&
                    size == sizeof(nk::u32)) {
                    m_point_light_count = *static_cast<const nk::u32*>(data);
                } else if (
                    uniform == nk::builtin_shader_uniform::point_lights &&
                    size == sizeof(m_point_lights)) {
                    std::memcpy(m_point_lights, data, size);
                } else if (
                    uniform == nk::builtin_shader_uniform::render_view_mode &&
                    size == sizeof(nk::u32)) {
                    m_uploaded_render_view_mode =
                        *static_cast<const nk::u32*>(data);
                }
            }
            if (uniform == nk::builtin_shader_uniform::projection ||
                uniform == nk::builtin_shader_uniform::view) {
                append_shader_trace(3);
            } else if (uniform == nk::builtin_shader_uniform::model ||
                       uniform == nk::builtin_shader_uniform::normal_matrix) {
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
            ASSERT_LT(m_shader_trace_length, std::size(m_shader_trace));
            m_shader_trace[m_shader_trace_length++] = value;
        }

        BeginMode m_begin_mode;
        bool m_fail_end = false;
        bool m_fail_texture_create = false;
        bool m_fail_shader_create = false;
        nk::u16 m_next_shader_index = 0;
        nk::ShaderHandle m_world_test_shader{};
        nk::ShaderHandle m_ui_test_shader{};
        nk::u32 m_next_instance_id[16]{};
        nk::u32 m_created_shaders = 0;
        nk::u32 m_destroyed_shaders = 0;
        nk::u32 m_sampler_array_index = 0;
        nk::u32 m_world_global_updates = 0;
        nk::u32 m_ui_global_updates = 0;
        nk::u32 m_world_object_updates = 0;
        nk::u32 m_ui_object_updates = 0;
        nk::u32 m_pass_trace = 0;
        nk::u32 m_end_calls = 0;
        nk::u32 m_destroyed_textures = 0;
        nk::u32 m_created_textures = 0;
        nk::u32 m_texture_create_attempts = 0;
        nk::u32 m_failed_texture_create_call = 0;
        bool m_default_specular_is_black = false;
        bool m_default_normal_is_flat = false;
        nk::u32 m_failed_instance_acquire_call = 0;
        nk::u32 m_instance_acquire_attempts = 0;
        nk::u32 m_acquired_instances = 0;
        nk::u32 m_released_instances = 0;
        nk::u32 m_instance_apply_count = 0;
        nk::u32 m_instance_update_count = 0;
        nk::u32 m_created_geometries = 0;
        nk::u32 m_destroyed_geometries = 0;
        glm::vec4 m_ambient_color{};
        glm::vec3 m_directional_light_direction{};
        glm::vec4 m_directional_light_color{};
        glm::vec3 m_view_position_value{};
        nk::u32 m_point_light_count = 0;
        nk::PointLightUniform m_point_lights[nk::max_point_light_count]{};
        nk::u32 m_uploaded_render_view_mode = 0;
        nk::Texture* m_specular_sampler_texture = nullptr;
        nk::Texture* m_normal_sampler_texture = nullptr;
        nk::f32 m_shininess = 0.0f;
        nk::u8 m_shader_trace[64]{};
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

    struct TestRenderSystems {
        nk::mem::Allocator& allocator;
        TestRenderer& renderer;
        nk::ResourceSystem* resources = nullptr;
        nk::TextureSystem* textures = nullptr;
        nk::ShaderSystem* shaders = nullptr;
        nk::MaterialSystem* materials = nullptr;

        ~TestRenderSystems() { shutdown(); }

        bool init(const nk::u32 material_capacity = 8) {
            auto resources_created = nk::ResourceSystem::create(
                allocator,
                NK_TEST_ASSET_ROOT);
            if (!resources_created)
                return false;
            resources = *resources_created;

            auto textures_created = nk::TextureSystem::create(
                allocator,
                renderer,
                *resources,
                8);
            if (!textures_created)
                return false;
            textures = *textures_created;

            auto shaders_created = nk::ShaderSystem::create(
                allocator,
                renderer,
                *resources,
                {.max_shader_count = 4});
            if (!shaders_created)
                return false;
            shaders = *shaders_created;
            if (!shaders->load(nk::builtin_material_shader_name) ||
                !shaders->load(nk::builtin_ui_shader_name)) {
                return false;
            }

            auto materials_created = nk::MaterialSystem::create(
                allocator,
                *shaders,
                *textures,
                *resources,
                material_capacity);
            if (!materials_created)
                return false;
            materials = *materials_created;
            return true;
        }

        void shutdown() {
            if (materials != nullptr) {
                nk::MaterialSystem::destroy(allocator, materials);
                materials = nullptr;
            }
            if (shaders != nullptr) {
                nk::ShaderSystem::destroy(allocator, shaders);
                shaders = nullptr;
            }
            if (textures != nullptr) {
                nk::TextureSystem::destroy(allocator, textures);
                textures = nullptr;
            }
            if (resources != nullptr) {
                nk::ResourceSystem::destroy(allocator, resources);
                resources = nullptr;
            }
        }
    };
}

static_assert(std::is_trivially_copyable_v<nk::renderer_error>);
static_assert(sizeof(nk::renderer_error) == 8);
static_assert(nk::TextureUse::diffuse != nk::TextureUse::specular);
static_assert(nk::TextureUse::diffuse != nk::TextureUse::normal);
static_assert(nk::TextureUse::specular != nk::TextureUse::normal);

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
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init());

    auto frame = renderer.draw_frame(
        *systems.materials,
        {.delta_time = 1.0 / 60.0});

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
    TestRenderSystems begin_systems{allocator, begin_failure};
    ASSERT_TRUE(begin_systems.init());

    auto begun = begin_failure.draw_frame(
        *begin_systems.materials,
        {.delta_time = 1.0 / 60.0});
    ASSERT_FALSE(begun);
    EXPECT_EQ(begun.error().code, nk::renderer_error_code::fence_wait_failed);
    EXPECT_EQ(begun.error().native_code, VK_ERROR_DEVICE_LOST);
    EXPECT_EQ(begin_failure.frame_number(), 0);

    TestRenderer end_failure{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems end_systems{allocator, end_failure};
    ASSERT_TRUE(end_systems.init());
    end_failure.fail_end(true);
    nk::GeometryRenderData geometry{};
    auto ended = end_failure.draw_frame(*end_systems.materials, {
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
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init());
    nk::GeometryRenderData world_geometry{};
    nk::GeometryRenderData ui_geometry{};

    auto frame = renderer.draw_frame(*systems.materials, {
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

TEST(RendererResult, RoutesSceneLightingOnlyThroughTheWorldShader) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init());
    renderer.set_view(glm::mat4{1.0f}, glm::vec3{3.0f, 4.0f, 5.0f});
    ASSERT_TRUE(renderer.set_render_view_mode(
        nk::RenderViewMode::lighting_only));

    nk::Geometry world_geometry{};
    world_geometry.material = &systems.materials->default_material();
    const nk::GeometryRenderData world_data{
        .model = glm::mat4{1.0f},
        .geometry = &world_geometry,
    };

    nk::Geometry ui_geometry{};
    ui_geometry.material = &systems.materials->default_ui_material();
    const nk::GeometryRenderData ui_data{
        .model = glm::mat4{1.0f},
        .geometry = &ui_geometry,
    };

    auto frame = renderer.draw_frame(*systems.materials, {
        .delta_time = 1.0 / 60.0,
        .lighting = {
            .ambient_color = {0.1f, 0.2f, 0.3f, 1.0f},
            .directional = {
                .direction = {-1.0f, -2.0f, -3.0f},
                .color = {0.7f, 0.6f, 0.5f, 1.0f},
            },
            .point_lights = {
                {
                    .position = {-5.5f, 0.0f, -5.5f},
                    .color = {0.0f, 1.0f, 0.0f, 1.0f},
                    .constant = 1.0f,
                    .linear = 0.35f,
                    .quadratic = 0.44f,
                },
                {
                    .position = {5.5f, 0.0f, -5.5f},
                    .color = {1.0f, 0.0f, 0.0f, 1.0f},
                    .constant = 1.0f,
                    .linear = 0.35f,
                    .quadratic = 0.44f,
                },
            },
            .point_light_count = 2,
        },
        .geometry_count = 1,
        .geometries = &world_data,
        .ui_geometry_count = 1,
        .ui_geometries = &ui_data,
    });

    ASSERT_TRUE(frame);
    constexpr nk::u8 world_protocol[] = {
        1, 2, 3, 3, 6, 6, 6, 6, 6, 6, 6, 4, 5, 6, 7, 7, 7, 6, 8, 9, 9,
    };
    constexpr nk::u8 ui_protocol[] = {
        1, 2, 3, 3, 4, 5, 6, 7, 8, 9,
    };
    ASSERT_EQ(
        renderer.shader_trace_length(),
        std::size(world_protocol) + std::size(ui_protocol));
    for (nk::u32 operation = 0;
         operation < std::size(world_protocol);
         ++operation) {
        EXPECT_EQ(renderer.shader_trace(operation), world_protocol[operation]);
    }
    for (nk::u32 operation = 0;
         operation < std::size(ui_protocol);
         ++operation) {
        EXPECT_EQ(
            renderer.shader_trace(std::size(world_protocol) + operation),
            ui_protocol[operation]);
    }
    EXPECT_EQ(renderer.ambient_color(), glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
    EXPECT_EQ(
        renderer.directional_light_direction(),
        glm::vec3(-1.0f, -2.0f, -3.0f));
    EXPECT_EQ(
        renderer.directional_light_color(),
        glm::vec4(0.7f, 0.6f, 0.5f, 1.0f));
    EXPECT_EQ(renderer.view_position(), glm::vec3(3.0f, 4.0f, 5.0f));
    EXPECT_EQ(renderer.point_light_count(), 2u);
    EXPECT_EQ(renderer.point_light(0).position, glm::vec3(-5.5f, 0.0f, -5.5f));
    EXPECT_EQ(renderer.point_light(0).color, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    EXPECT_FLOAT_EQ(renderer.point_light(0).constant, 1.0f);
    EXPECT_FLOAT_EQ(renderer.point_light(0).linear, 0.35f);
    EXPECT_FLOAT_EQ(renderer.point_light(0).quadratic, 0.44f);
    EXPECT_EQ(renderer.point_light(1).position, glm::vec3(5.5f, 0.0f, -5.5f));
    EXPECT_EQ(renderer.point_light(1).color, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(
        renderer.uploaded_render_view_mode(),
        static_cast<nk::u32>(nk::RenderViewMode::lighting_only));
    EXPECT_EQ(renderer.pass_trace(), 1234u);
    EXPECT_EQ(renderer.world_global_updates(), 1u);
    EXPECT_EQ(renderer.ui_global_updates(), 1u);
    EXPECT_EQ(renderer.world_object_updates(), 1u);
    EXPECT_EQ(renderer.ui_object_updates(), 1u);
}

TEST(RendererLighting, ComputesFinitePointAttenuation) {
    const nk::PointLight light{
        .constant = 1.0f,
        .linear = 0.35f,
        .quadratic = 0.44f,
    };

    EXPECT_FLOAT_EQ(light.attenuation(0.0f), 1.0f);
    EXPECT_NEAR(light.attenuation(2.0f), 1.0f / 3.46f, 0.00001f);
    EXPECT_FLOAT_EQ(light.attenuation(-1.0f), 0.0f);

    nk::PointLight invalid = light;
    invalid.constant = 0.0f;
    EXPECT_FALSE(invalid.valid());
    EXPECT_FLOAT_EQ(invalid.attenuation(2.0f), 0.0f);
}

TEST(RendererLighting, PreservesTheLastValidRenderViewMode) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};

    EXPECT_EQ(
        renderer.render_view_mode(),
        nk::RenderViewMode::default_lit);
    EXPECT_TRUE(renderer.set_render_view_mode(
        nk::RenderViewMode::lighting_only));
    EXPECT_EQ(
        renderer.render_view_mode(),
        nk::RenderViewMode::lighting_only);
    EXPECT_FALSE(renderer.set_render_view_mode(
        static_cast<nk::RenderViewMode>(3)));
    EXPECT_EQ(
        renderer.render_view_mode(),
        nk::RenderViewMode::lighting_only);
}

TEST(RendererLighting, RoutesPortableNumberKeysThroughEvents) {
    nk::EventSystem::shutdown();
    nk::EventSystem::init();

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    ASSERT_TRUE(nk::EventSystem::register_event(
        nk::SystemEventCode::SetRenderViewMode,
        &renderer,
        nk::on_render_view_mode));

    nk::RenderViewMode mode{};
    ASSERT_TRUE(nk::render_view_mode_from_key(nk::KeyCode::Num2, mode));
    EXPECT_EQ(mode, nk::RenderViewMode::normals);
    EXPECT_FALSE(nk::render_view_mode_from_key(nk::KeyCode::T, mode));

    nk::EventContext mode_context{};
    mode_context.data.u32[0] = static_cast<nk::u32>(mode);
    EXPECT_TRUE(nk::EventSystem::fire_event(
        nk::SystemEventCode::SetRenderViewMode,
        nullptr,
        mode_context));
    EXPECT_EQ(renderer.render_view_mode(), nk::RenderViewMode::normals);

    ASSERT_TRUE(nk::render_view_mode_from_key(nk::KeyCode::Num0, mode));
    mode_context.data.u32[0] = static_cast<nk::u32>(mode);
    EXPECT_TRUE(nk::EventSystem::fire_event(
        nk::SystemEventCode::SetRenderViewMode,
        nullptr,
        mode_context));
    EXPECT_EQ(renderer.render_view_mode(), nk::RenderViewMode::default_lit);

    EXPECT_TRUE(nk::EventSystem::unregister_event(
        nk::SystemEventCode::SetRenderViewMode,
        &renderer,
        nk::on_render_view_mode));
    nk::EventSystem::shutdown();
}

TEST(MaterialSystem, RejectsInvalidPointLightConfiguration) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init());

    nk::SceneLighting lighting{};
    lighting.point_light_count = nk::max_point_light_count + 1;
    auto excessive = systems.materials->apply_global(
        nk::MaterialType::world,
        glm::mat4{1.0f},
        glm::mat4{1.0f},
        glm::vec3{0.0f},
        lighting);
    ASSERT_FALSE(excessive);
    EXPECT_EQ(excessive.error().code, nk::material_error_code::invalid_config);

    lighting.point_light_count = 1;
    lighting.point_lights[0].constant = 0.0f;
    auto invalid = systems.materials->apply_global(
        nk::MaterialType::world,
        glm::mat4{1.0f},
        glm::mat4{1.0f},
        glm::vec3{0.0f},
        lighting);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, nk::material_error_code::invalid_config);

    lighting.point_light_count = 0;
    auto invalid_mode = systems.materials->apply_global(
        nk::MaterialType::world,
        glm::mat4{1.0f},
        glm::mat4{1.0f},
        glm::vec3{0.0f},
        lighting,
        static_cast<nk::RenderViewMode>(3));
    ASSERT_FALSE(invalid_mode);
    EXPECT_EQ(
        invalid_mode.error().code,
        nk::material_error_code::invalid_config);
    EXPECT_EQ(renderer.shader_trace_length(), 0u);
}

TEST(RendererResult, AdvancesFrameOnlyAfterSuccessfulPresentation) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init());

    auto frame = renderer.draw_frame(
        *systems.materials,
        {.delta_time = 1.0 / 60.0});

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

TEST(ShaderSystem, RoutesSamplerArrayElementsWithoutTemporaryNames) {
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
            .max_shader_count = 1,
            .max_uniform_count = 8,
            .max_global_samplers = 4,
            .max_instance_samplers = 0,
        });
    ASSERT_TRUE(shaders_created);
    nk::ShaderSystem* shaders = *shaders_created;

    auto shader = shaders->load("Test.Array");
    ASSERT_TRUE(shader);
    auto textures = shaders->uniform(*shader, "textures");
    ASSERT_TRUE(textures);
    ASSERT_TRUE(shaders->use(*shader));
    ASSERT_TRUE(shaders->bind_globals());
    ASSERT_TRUE(shaders->set_sampler(*textures, nullptr, 2));
    EXPECT_EQ(renderer.sampler_array_index(), 2u);

    auto out_of_bounds = shaders->set_sampler(*textures, nullptr, 3);
    ASSERT_FALSE(out_of_bounds);
    EXPECT_EQ(
        out_of_bounds.error().code,
        nk::shader_system_error_code::invalid_uniform);

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
    EXPECT_TRUE(textures->default_texture().valid());
    EXPECT_TRUE(textures->default_specular_texture().valid());
    EXPECT_TRUE(textures->default_normal_texture().valid());
    EXPECT_TRUE(renderer.default_specular_is_black());
    EXPECT_TRUE(renderer.default_normal_is_flat());
    EXPECT_EQ(renderer.created_textures(), 3u);

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
    EXPECT_EQ(renderer.destroyed_textures(), 4u);
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
    EXPECT_EQ(renderer.destroyed_textures(), 3u);
}

TEST(TextureSystem, RollsBackWhenDefaultSpecularTextureCreationFails) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    renderer.fail_texture_create_on_call(2);
    auto resources_created = nk::ResourceSystem::create(
        allocator, NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;

    auto created = nk::TextureSystem::create(
        allocator, renderer, *resources, 2);

    ASSERT_FALSE(created);
    EXPECT_EQ(created.error().code, nk::texture_error_code::renderer_failed);
    EXPECT_EQ(renderer.created_textures(), 1u);
    EXPECT_EQ(renderer.destroyed_textures(), 1u);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(TextureSystem, RollsBackWhenDefaultNormalTextureCreationFails) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    renderer.fail_texture_create_on_call(3);
    auto resources_created = nk::ResourceSystem::create(
        allocator, NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;

    auto created = nk::TextureSystem::create(
        allocator, renderer, *resources, 2);

    ASSERT_FALSE(created);
    EXPECT_EQ(created.error().code, nk::texture_error_code::renderer_failed);
    EXPECT_EQ(renderer.created_textures(), 2u);
    EXPECT_EQ(renderer.destroyed_textures(), 2u);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
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
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));
    nk::TextureSystem* textures = systems.textures;
    nk::MaterialSystem* materials = systems.materials;
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
    ASSERT_NE((*first)->specular_map.texture, nullptr);
    EXPECT_EQ((*first)->specular_map.use, nk::TextureUse::specular);
    EXPECT_EQ((*first)->specular_map.texture->width, 480u);
    ASSERT_NE((*first)->normal_map.texture, nullptr);
    EXPECT_EQ((*first)->normal_map.use, nk::TextureUse::normal);
    EXPECT_EQ((*first)->normal_map.texture->width, 480u);
    EXPECT_FLOAT_EQ((*first)->shininess, 64.0f);
    EXPECT_EQ(textures->reference_count("paving"), 1u);
    EXPECT_EQ(textures->reference_count("paving_SPEC"), 1u);
    EXPECT_EQ(textures->reference_count("paving_NRM"), 1u);

    auto second = materials->acquire("test_material");
    ASSERT_TRUE(second);
    EXPECT_EQ(*first, *second);
    EXPECT_EQ(materials->reference_count("test_material"), 2u);
    EXPECT_EQ(textures->reference_count("paving"), 1u);
    EXPECT_EQ(textures->reference_count("paving_SPEC"), 1u);
    EXPECT_EQ(textures->reference_count("paving_NRM"), 1u);

    materials->release("test_material");
    EXPECT_EQ(materials->loaded_count(), 1u);
    materials->release("test_material");
    EXPECT_EQ(materials->loaded_count(), 0u);
    EXPECT_EQ(textures->loaded_count(), 0u);
    EXPECT_EQ(renderer.released_instances(), 2u);

    systems.shutdown();
    EXPECT_EQ(renderer.released_instances(), 4u);
}

TEST(MaterialSystem, UsesNonOwnedDefaultsForOmittedMaterialMaps) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));

    nk::MaterialConfig config{};
    ASSERT_TRUE(config.name.assign("default_maps"));
    auto acquired = systems.materials->acquire(config);

    ASSERT_TRUE(acquired);
    EXPECT_EQ(
        (*acquired)->diffuse_map.texture,
        &systems.textures->default_texture());
    EXPECT_EQ(
        (*acquired)->specular_map.texture,
        &systems.textures->default_specular_texture());
    EXPECT_EQ(
        (*acquired)->specular_map_name.view(),
        nk::default_specular_texture_name);
    EXPECT_EQ(
        (*acquired)->normal_map.texture,
        &systems.textures->default_normal_texture());
    EXPECT_EQ(
        (*acquired)->normal_map_name.view(),
        nk::default_normal_texture_name);
    EXPECT_EQ(systems.textures->loaded_count(), 0u);
    systems.materials->release("default_maps");
    EXPECT_EQ(systems.textures->loaded_count(), 0u);
}

TEST(MaterialSystem, RollsBackDiffuseWhenSpecularAcquisitionFails) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));

    nk::MaterialConfig config{};
    ASSERT_TRUE(config.name.assign("transactional_maps"));
    ASSERT_TRUE(config.diffuse_map_name.assign("paving"));
    ASSERT_TRUE(config.specular_map_name.assign("missing_SPEC"));
    auto acquired = systems.materials->acquire(config);

    ASSERT_FALSE(acquired);
    EXPECT_EQ(acquired.error().code, nk::material_error_code::texture_failed);
    EXPECT_EQ(systems.materials->loaded_count(), 0u);
    EXPECT_EQ(systems.textures->loaded_count(), 0u);
    EXPECT_EQ(systems.textures->reference_count("paving"), 0u);
    EXPECT_EQ(renderer.destroyed_textures(), 1u);
}

TEST(MaterialSystem, RollsBackMapsWhenNormalAcquisitionFails) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));

    nk::MaterialConfig config{};
    ASSERT_TRUE(config.name.assign("transactional_normal_map"));
    ASSERT_TRUE(config.diffuse_map_name.assign("paving"));
    ASSERT_TRUE(config.specular_map_name.assign("paving_SPEC"));
    ASSERT_TRUE(config.normal_map_name.assign("missing_NRM"));
    auto acquired = systems.materials->acquire(config);

    ASSERT_FALSE(acquired);
    EXPECT_EQ(acquired.error().code, nk::material_error_code::texture_failed);
    EXPECT_EQ(systems.materials->loaded_count(), 0u);
    EXPECT_EQ(systems.textures->loaded_count(), 0u);
    EXPECT_EQ(systems.textures->reference_count("paving"), 0u);
    EXPECT_EQ(systems.textures->reference_count("paving_SPEC"), 0u);
    EXPECT_EQ(renderer.destroyed_textures(), 2u);
}

TEST(MaterialSystem, RebindsMaterialPropertiesWithoutLeakingReferences) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));
    nk::TextureSystem* textures = systems.textures;
    nk::MaterialSystem* materials = systems.materials;
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

    ASSERT_TRUE(materials->set_specular_texture(
        *(*material),
        "cobblestone_SPEC"));
    EXPECT_EQ(
        (*material)->specular_map_name.view(),
        nk::strview{"cobblestone_SPEC"});
    EXPECT_EQ(textures->reference_count("paving_SPEC"), 0u);
    EXPECT_EQ(textures->reference_count("cobblestone_SPEC"), 1u);

    auto missing = materials->set_specular_texture(
        *(*material),
        "missing_SPEC");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, nk::material_error_code::texture_failed);
    EXPECT_EQ(
        (*material)->specular_map_name.view(),
        nk::strview{"cobblestone_SPEC"});
    EXPECT_EQ(textures->reference_count("cobblestone_SPEC"), 1u);

    ASSERT_TRUE(materials->set_specular_texture(
        *(*material),
        nk::default_specular_texture_name));
    EXPECT_EQ(textures->reference_count("cobblestone_SPEC"), 0u);

    ASSERT_TRUE(materials->set_normal_texture(
        *(*material),
        "cobblestone_NRM"));
    EXPECT_EQ(
        (*material)->normal_map_name.view(),
        nk::strview{"cobblestone_NRM"});
    EXPECT_EQ(textures->reference_count("paving_NRM"), 0u);
    EXPECT_EQ(textures->reference_count("cobblestone_NRM"), 1u);

    auto missing_normal = materials->set_normal_texture(
        *(*material),
        "missing_NRM");
    ASSERT_FALSE(missing_normal);
    EXPECT_EQ(
        missing_normal.error().code,
        nk::material_error_code::texture_failed);
    EXPECT_EQ(
        (*material)->normal_map_name.view(),
        nk::strview{"cobblestone_NRM"});
    EXPECT_EQ(textures->reference_count("cobblestone_NRM"), 1u);

    ASSERT_TRUE(materials->set_normal_texture(
        *(*material),
        nk::default_normal_texture_name));
    EXPECT_EQ(textures->reference_count("cobblestone_NRM"), 0u);
    ASSERT_TRUE(materials->set_shininess(*(*material), 96.0f));
    EXPECT_FLOAT_EQ((*material)->shininess, 96.0f);

    auto invalid_shininess =
        materials->set_shininess(*(*material), 0.0f);
    ASSERT_FALSE(invalid_shininess);
    EXPECT_EQ(
        invalid_shininess.error().code,
        nk::material_error_code::invalid_config);
    EXPECT_FLOAT_EQ((*material)->shininess, 96.0f);

    systems.shutdown();
}

TEST(MaterialSystem, RebindsTextureMapsTransactionally) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));
    auto material = systems.materials->acquire("test_material");
    ASSERT_TRUE(material);

    const nk::u32 initial_generation = (*material)->generation;
    auto failed = systems.materials->set_texture_maps(
        *(*material),
        "cobblestone",
        "missing_SPEC",
        "cobblestone_NRM");
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code, nk::material_error_code::texture_failed);
    EXPECT_EQ((*material)->diffuse_map_name.view(), nk::strview{"paving"});
    EXPECT_EQ(
        (*material)->specular_map_name.view(),
        nk::strview{"paving_SPEC"});
    EXPECT_EQ(
        (*material)->normal_map_name.view(),
        nk::strview{"paving_NRM"});
    EXPECT_EQ((*material)->generation, initial_generation);
    EXPECT_EQ(systems.textures->reference_count("cobblestone"), 0u);
    EXPECT_EQ(systems.textures->reference_count("paving"), 1u);
    EXPECT_EQ(systems.textures->reference_count("paving_SPEC"), 1u);
    EXPECT_EQ(systems.textures->reference_count("paving_NRM"), 1u);

    failed = systems.materials->set_texture_maps(
        *(*material),
        "cobblestone",
        "cobblestone_SPEC",
        "missing_NRM");
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code, nk::material_error_code::texture_failed);
    EXPECT_EQ((*material)->diffuse_map_name.view(), nk::strview{"paving"});
    EXPECT_EQ(
        (*material)->specular_map_name.view(),
        nk::strview{"paving_SPEC"});
    EXPECT_EQ(
        (*material)->normal_map_name.view(),
        nk::strview{"paving_NRM"});
    EXPECT_EQ((*material)->generation, initial_generation);
    EXPECT_EQ(systems.textures->reference_count("cobblestone"), 0u);
    EXPECT_EQ(systems.textures->reference_count("cobblestone_SPEC"), 0u);

    ASSERT_TRUE(systems.materials->set_texture_maps(
        *(*material),
        "cobblestone",
        "cobblestone_SPEC",
        "cobblestone_NRM"));
    EXPECT_EQ(
        (*material)->diffuse_map_name.view(),
        nk::strview{"cobblestone"});
    EXPECT_EQ(
        (*material)->specular_map_name.view(),
        nk::strview{"cobblestone_SPEC"});
    EXPECT_EQ(
        (*material)->normal_map_name.view(),
        nk::strview{"cobblestone_NRM"});
    EXPECT_EQ((*material)->generation, initial_generation + 1);
    EXPECT_EQ(systems.textures->reference_count("paving"), 0u);
    EXPECT_EQ(systems.textures->reference_count("paving_SPEC"), 0u);
    EXPECT_EQ(systems.textures->reference_count("paving_NRM"), 0u);
    EXPECT_EQ(systems.textures->reference_count("cobblestone"), 1u);
    EXPECT_EQ(systems.textures->reference_count("cobblestone_SPEC"), 1u);
    EXPECT_EQ(systems.textures->reference_count("cobblestone_NRM"), 1u);

    systems.materials->release("test_material");
    systems.shutdown();
}

TEST(MaterialSystem, UpdatesInstanceDataOncePerFrameAndInvalidatesChanges) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));

    auto material = systems.materials->acquire("test_material");
    ASSERT_TRUE(material);
    ASSERT_TRUE(systems.materials->apply_global(
        nk::MaterialType::world,
        glm::mat4{1.0f},
        glm::mat4{1.0f},
        glm::vec3{0.0f},
        nk::SceneLighting{}));

    ASSERT_TRUE(systems.materials->apply_instance(*(*material), 7));
    ASSERT_TRUE(systems.materials->apply_instance(*(*material), 7));
    EXPECT_EQ(renderer.instance_apply_count(), 2u);
    EXPECT_EQ(renderer.instance_update_count(), 1u);
    EXPECT_EQ(renderer.specular_sampler_texture(), (*material)->specular_map.texture);
    EXPECT_EQ(renderer.normal_sampler_texture(), (*material)->normal_map.texture);
    EXPECT_FLOAT_EQ(renderer.shininess(), 64.0f);

    ASSERT_TRUE(systems.materials->set_diffuse_color(
        *(*material),
        glm::vec4{0.5f}));
    ASSERT_TRUE(systems.materials->apply_instance(*(*material), 7));
    EXPECT_EQ(renderer.instance_update_count(), 2u);

    ASSERT_TRUE(systems.materials->set_diffuse_texture(
        *(*material),
        "cobblestone"));
    ASSERT_TRUE(systems.materials->apply_instance(*(*material), 7));
    EXPECT_EQ(renderer.instance_update_count(), 3u);

    const nk::u32 instance_id = (*material)->internal_id;
    (*material)->internal_id = instance_id + 1;
    ASSERT_TRUE(systems.materials->apply_instance(*(*material), 7));
    EXPECT_EQ(renderer.instance_update_count(), 4u);
    (*material)->internal_id = instance_id;

    ASSERT_TRUE(systems.materials->apply_instance(*(*material), 8));
    EXPECT_EQ(renderer.instance_apply_count(), 6u);
    EXPECT_EQ(renderer.instance_update_count(), 5u);

    systems.materials->release("test_material");
    systems.shutdown();
}

TEST(MaterialSystem, RollsBackDefaultInstancesWhenInitializationFails) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};

    auto resources_created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(resources_created);
    nk::ResourceSystem* resources = *resources_created;
    auto textures_created = nk::TextureSystem::create(
        allocator,
        renderer,
        *resources,
        8);
    ASSERT_TRUE(textures_created);
    nk::TextureSystem* textures = *textures_created;
    auto shaders_created = nk::ShaderSystem::create(
        allocator,
        renderer,
        *resources,
        {.max_shader_count = 4});
    ASSERT_TRUE(shaders_created);
    nk::ShaderSystem* shaders = *shaders_created;
    ASSERT_TRUE(shaders->load(nk::builtin_material_shader_name));
    ASSERT_TRUE(shaders->load(nk::builtin_ui_shader_name));

    const nk::u64 allocations_before =
        allocator.get_active_allocation_count();
    renderer.fail_instance_acquire_on_call(2);
    auto materials = nk::MaterialSystem::create(
        allocator,
        *shaders,
        *textures,
        *resources,
        4);
    ASSERT_FALSE(materials);
    EXPECT_EQ(
        materials.error().code,
        nk::material_error_code::out_of_memory);
    EXPECT_EQ(renderer.acquired_instances(), 1u);
    EXPECT_EQ(renderer.released_instances(), 1u);
    EXPECT_EQ(
        allocator.get_active_allocation_count(),
        allocations_before);

    nk::ShaderSystem::destroy(allocator, shaders);
    nk::TextureSystem::destroy(allocator, textures);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(MaterialSystem, RejectsShadersThatDoNotMatchTheMaterialType) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));

    nk::MaterialConfig config{};
    ASSERT_TRUE(config.name.assign("mismatched_shader"));
    ASSERT_TRUE(config.shader_name.assign(nk::builtin_ui_shader_name));
    ASSERT_TRUE(config.diffuse_map_name.assign("paving"));
    config.type = nk::MaterialType::world;
    const nk::u32 acquired_before = renderer.acquired_instances();

    auto material = systems.materials->acquire(config);
    ASSERT_FALSE(material);
    EXPECT_EQ(
        material.error().code,
        nk::material_error_code::invalid_config);
    EXPECT_EQ(renderer.acquired_instances(), acquired_before);
    EXPECT_EQ(systems.materials->loaded_count(), 0u);
    EXPECT_EQ(systems.textures->loaded_count(), 0u);

    systems.shutdown();
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
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
    EXPECT_EQ(plane->vertices[0].normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(plane->vertices[0].texcoord, glm::vec2(0.0f, 0.0f));
    EXPECT_EQ(plane->vertices[0].tangent, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(plane->vertices[21].position, glm::vec3(5.0f, 3.0f, 0.0f));
    EXPECT_EQ(plane->vertices[21].normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(plane->vertices[21].texcoord, glm::vec2(4.0f, 3.0f));
}

TEST(GeometrySystem, GeneratesCubeWithOutwardUnitNormals) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto cube = nk::GeometrySystem::generate_cube(
        allocator,
        4.0f,
        6.0f,
        8.0f,
        2.0f,
        3.0f,
        "cube",
        "test_material");
    ASSERT_TRUE(cube);
    ASSERT_EQ(cube->vertices.length(), 24u);
    ASSERT_EQ(cube->indices.length(), 36u);
    EXPECT_EQ(cube->vertices[0].position, glm::vec3(-2.0f, -3.0f, 4.0f));
    EXPECT_EQ(cube->vertices[1].texcoord, glm::vec2(2.0f, 3.0f));

    for (nk::u64 index = 0; index < cube->indices.length(); index += 3) {
        const glm::Vertex3D& a = cube->vertices[cube->indices[index]];
        const glm::Vertex3D& b = cube->vertices[cube->indices[index + 1]];
        const glm::Vertex3D& c = cube->vertices[cube->indices[index + 2]];
        const glm::vec3 winding_normal = glm::cross(
            b.position - a.position,
            c.position - a.position);
        EXPECT_GT(glm::dot(winding_normal, a.normal), 0.0f);
        EXPECT_FLOAT_EQ(glm::dot(a.normal, a.normal), 1.0f);
        EXPECT_NEAR(glm::dot(glm::vec3{a.tangent}, a.normal), 0.0f, 1.0e-6f);
        EXPECT_NEAR(
            glm::dot(glm::vec3{a.tangent}, glm::vec3{a.tangent}),
            1.0f,
            1.0e-6f);
        EXPECT_TRUE(a.tangent.w == -1.0f || a.tangent.w == 1.0f);
        EXPECT_EQ(a.normal, b.normal);
        EXPECT_EQ(a.normal, c.normal);
    }
}

TEST(GeometrySystem, GeneratesNormalsWithoutNanForDegenerateTriangles) {
    glm::Vertex3D vertices[]{
        {.position = {0.0f, 0.0f, 0.0f}, .normal = glm::vec3{9.0f},
         .texcoord = {}, .tangent = {}},
        {.position = {1.0f, 0.0f, 0.0f}, .normal = glm::vec3{9.0f},
         .texcoord = {}, .tangent = {}},
        {.position = {0.0f, 1.0f, 0.0f}, .normal = glm::vec3{9.0f},
         .texcoord = {}, .tangent = {}},
        {.position = {2.0f, 0.0f, 0.0f}, .normal = glm::vec3{9.0f},
         .texcoord = {}, .tangent = {}},
    };
    const nk::u32 indices[]{0, 1, 2, 1, 3, 3};

    auto generated = nk::GeometrySystem::generate_normals(vertices, indices);

    ASSERT_TRUE(generated);
    EXPECT_EQ(vertices[0].normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(vertices[1].normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(vertices[2].normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(vertices[3].normal, glm::vec3(0.0f));
    for (const glm::Vertex3D& vertex : vertices) {
        EXPECT_TRUE(std::isfinite(vertex.normal.x));
        EXPECT_TRUE(std::isfinite(vertex.normal.y));
        EXPECT_TRUE(std::isfinite(vertex.normal.z));
    }
}

TEST(GeometrySystem, RejectsInvalidTopologyWithoutChangingNormals) {
    glm::Vertex3D vertices[]{
        {.position = {}, .normal = {1.0f, 2.0f, 3.0f}, .texcoord = {},
         .tangent = {}},
        {.position = {}, .normal = {4.0f, 5.0f, 6.0f}, .texcoord = {},
         .tangent = {}},
    };
    const nk::u32 indices[]{0, 1, 2};

    auto generated = nk::GeometrySystem::generate_normals(vertices, indices);

    ASSERT_FALSE(generated);
    EXPECT_EQ(generated.error().code, nk::geometry_error_code::invalid_config);
    EXPECT_EQ(vertices[0].normal, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(vertices[1].normal, glm::vec3(4.0f, 5.0f, 6.0f));
}

TEST(GeometrySystem, GeneratesFiniteTangentsForDegenerateUvs) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    glm::Vertex3D vertices[]{
        {.position = {0.0f, 0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {0.0f, 0.0f}, .tangent = glm::vec4{9.0f}},
        {.position = {1.0f, 0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {0.0f, 0.0f}, .tangent = glm::vec4{9.0f}},
        {.position = {0.0f, 1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {0.0f, 0.0f}, .tangent = glm::vec4{9.0f}},
    };
    const nk::u32 indices[]{0, 1, 2};

    auto generated = nk::GeometrySystem::generate_tangents(
        allocator,
        vertices,
        indices);

    ASSERT_TRUE(generated);
    for (const glm::Vertex3D& vertex : vertices) {
        EXPECT_EQ(vertex.tangent, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    }
}

TEST(GeometrySystem, PreservesMirroredUvHandedness) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    glm::Vertex3D vertices[]{
        {.position = {0.0f, 0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {0.0f, 0.0f}, .tangent = {}},
        {.position = {1.0f, 0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {0.0f, 1.0f}, .tangent = {}},
        {.position = {0.0f, 1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {1.0f, 0.0f}, .tangent = {}},
    };
    const nk::u32 indices[]{0, 1, 2};

    auto generated = nk::GeometrySystem::generate_tangents(
        allocator,
        vertices,
        indices);

    ASSERT_TRUE(generated);
    for (const glm::Vertex3D& vertex : vertices) {
        EXPECT_FLOAT_EQ(vertex.tangent.w, -1.0f);
        EXPECT_NEAR(
            glm::dot(glm::vec3{vertex.tangent}, vertex.normal),
            0.0f,
            1.0e-6f);
    }
}

TEST(GeometrySystem, RejectsInvalidTopologyWithoutChangingTangents) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    glm::Vertex3D vertices[]{
        {.position = {}, .normal = {0.0f, 0.0f, 1.0f}, .texcoord = {},
         .tangent = {1.0f, 2.0f, 3.0f, 4.0f}},
        {.position = {}, .normal = {0.0f, 0.0f, 1.0f}, .texcoord = {},
         .tangent = {5.0f, 6.0f, 7.0f, 8.0f}},
    };
    const nk::u32 indices[]{0, 1, 2};

    auto generated = nk::GeometrySystem::generate_tangents(
        allocator,
        vertices,
        indices);

    ASSERT_FALSE(generated);
    EXPECT_EQ(generated.error().code, nk::geometry_error_code::invalid_config);
    EXPECT_EQ(vertices[0].tangent, glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    EXPECT_EQ(vertices[1].tangent, glm::vec4(5.0f, 6.0f, 7.0f, 8.0f));
}

TEST(GeometrySystem, PreservesTangentsWhenScratchAllocationFails) {
    FailingAllocator allocator;
    glm::Vertex3D vertices[]{
        {.position = {0.0f, 0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {0.0f, 0.0f}, .tangent = {1.0f, 2.0f, 3.0f, 4.0f}},
        {.position = {1.0f, 0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {1.0f, 0.0f}, .tangent = {5.0f, 6.0f, 7.0f, 8.0f}},
        {.position = {0.0f, 1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f},
         .texcoord = {0.0f, 1.0f}, .tangent = {9.0f, 10.0f, 11.0f, 12.0f}},
    };
    const nk::u32 indices[]{0, 1, 2};

    auto generated = nk::GeometrySystem::generate_tangents(
        allocator,
        vertices,
        indices);

    ASSERT_FALSE(generated);
    EXPECT_EQ(generated.error().code, nk::geometry_error_code::out_of_memory);
    EXPECT_EQ(vertices[0].tangent, glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
    EXPECT_EQ(vertices[1].tangent, glm::vec4(5.0f, 6.0f, 7.0f, 8.0f));
    EXPECT_EQ(vertices[2].tangent, glm::vec4(9.0f, 10.0f, 11.0f, 12.0f));
}

static_assert(sizeof(glm::Vertex3D) == 48);
static_assert(offsetof(glm::Vertex3D, position) == 0);
static_assert(offsetof(glm::Vertex3D, normal) == 12);
static_assert(offsetof(glm::Vertex3D, texcoord) == 24);
static_assert(offsetof(glm::Vertex3D, tangent) == 32);

TEST(GeometrySystem, OwnsGeometryAndMaterialReferencesUntilFinalRelease) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));
    nk::MaterialSystem* materials = systems.materials;
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
    systems.shutdown();
    EXPECT_EQ(renderer.destroyed_geometries(), 5u);
}

TEST(Mesh, OwnsMultipleGeometryReferencesUntilDestruction) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));
    auto geometries_created = nk::GeometrySystem::create(
        allocator,
        renderer,
        *systems.materials,
        2);
    ASSERT_TRUE(geometries_created);
    nk::GeometrySystem* geometries = *geometries_created;

    {
        auto first = nk::GeometrySystem::generate_cube(
            allocator,
            2.0f,
            2.0f,
            2.0f,
            1.0f,
            1.0f,
            "mesh_first",
            "test_material");
        auto second = nk::GeometrySystem::generate_plane(
            allocator,
            2.0f,
            2.0f,
            1,
            1,
            1.0f,
            1.0f,
            "mesh_second",
            "test_material");
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        nk::GeometryConfig configs[]{
            std::move(*first),
            std::move(*second),
        };
        const glm::mat4 model = glm::translate(
            glm::mat4{1.0f},
            glm::vec3{3.0f, 2.0f, 1.0f});

        {
            auto mesh = nk::Mesh::create(
                allocator,
                *geometries,
                configs,
                model);
            ASSERT_TRUE(mesh);
            EXPECT_TRUE(mesh->valid());
            EXPECT_EQ(mesh->geometry_count(), 2u);
            ASSERT_NE(mesh->geometry(0), nullptr);
            ASSERT_NE(mesh->geometry(1), nullptr);
            EXPECT_EQ(
                mesh->geometry(0)->name.view(),
                nk::strview{"mesh_first"});
            EXPECT_EQ(
                mesh->geometry(1)->name.view(),
                nk::strview{"mesh_second"});
            EXPECT_EQ(mesh->model(), model);
            EXPECT_EQ(geometries->loaded_count(), 2u);
            EXPECT_EQ(
                systems.materials->reference_count("test_material"),
                2u);
        }

        EXPECT_EQ(geometries->loaded_count(), 0u);
        EXPECT_EQ(systems.materials->loaded_count(), 0u);
        EXPECT_EQ(renderer.destroyed_geometries(), 2u);
    }

    nk::GeometrySystem::destroy(allocator, geometries);
    systems.shutdown();
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(Mesh, SupportsEmptyGeometryGroups) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(2));
    auto geometries_created = nk::GeometrySystem::create(
        allocator,
        renderer,
        *systems.materials,
        1);
    ASSERT_TRUE(geometries_created);
    nk::GeometrySystem* geometries = *geometries_created;

    {
        auto mesh = nk::Mesh::create(allocator, *geometries, {});
        ASSERT_TRUE(mesh);
        EXPECT_TRUE(mesh->valid());
        EXPECT_EQ(mesh->geometry_count(), 0u);
        EXPECT_EQ(mesh->geometry(0), nullptr);
        EXPECT_TRUE(mesh->geometries().empty());
        EXPECT_EQ(geometries->loaded_count(), 0u);
    }

    nk::GeometrySystem::destroy(allocator, geometries);
    systems.shutdown();
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(Mesh, ReleasesCompletedAcquisitionsWhenCreationFailsPartially) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    TestRenderSystems systems{allocator, renderer};
    ASSERT_TRUE(systems.init(4));
    auto geometries_created = nk::GeometrySystem::create(
        allocator,
        renderer,
        *systems.materials,
        1);
    ASSERT_TRUE(geometries_created);
    nk::GeometrySystem* geometries = *geometries_created;

    {
        auto first = nk::GeometrySystem::generate_plane(
            allocator,
            2.0f,
            2.0f,
            1,
            1,
            1.0f,
            1.0f,
            "partial_first",
            "test_material");
        auto second = nk::GeometrySystem::generate_plane(
            allocator,
            4.0f,
            4.0f,
            1,
            1,
            1.0f,
            1.0f,
            "partial_second",
            "test_material");
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        nk::GeometryConfig configs[]{
            std::move(*first),
            std::move(*second),
        };

        auto mesh = nk::Mesh::create(allocator, *geometries, configs);

        ASSERT_FALSE(mesh);
        EXPECT_EQ(mesh.error().code, nk::mesh_error_code::geometry_failed);
        EXPECT_EQ(mesh.error().geometry_index, 1u);
        EXPECT_EQ(
            mesh.error().geometry_error,
            static_cast<nk::u32>(
                nk::geometry_error_code::capacity_exceeded));
        EXPECT_EQ(geometries->loaded_count(), 0u);
        EXPECT_EQ(systems.materials->loaded_count(), 0u);
        EXPECT_EQ(renderer.destroyed_geometries(), 1u);
    }

    nk::GeometrySystem::destroy(allocator, geometries);
    systems.shutdown();
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}
