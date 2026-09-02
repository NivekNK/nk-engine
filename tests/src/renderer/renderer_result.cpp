#include <gtest/gtest.h>

#include <type_traits>

#include "memory/malloc_allocator.h"
#include "renderer/renderer.h"
#include "renderer/vulkan/swapchain.h"
#include "renderer/vulkan/vulkan_renderer.h"

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
        const nk::Texture& diffuse_texture() const { return m_diffuse_texture; }

        nk::result<void, nk::renderer_error> create_texture(
            nk::strview,
            bool,
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

        void update_object(nk::GeometryRenderData) override {
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
    auto ended = end_failure.draw_frame({.delta_time = 1.0 / 60.0});
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
        "test", false, 1, 1, 4, pixel, false, &output);

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

TEST(RendererResult, LoadsAndReplacesTexturesTransactionally) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    nk::Texture texture{
        .width = 1,
        .height = 1,
        .channel_count = 4,
        .generation = 7,
        .m_internal_data = reinterpret_cast<void*>(0x1),
    };

    renderer.fail_texture_create(true);
    auto failed = renderer.load_texture("cobblestone", texture);
    ASSERT_FALSE(failed);
    EXPECT_EQ(
        failed.error().code,
        nk::renderer_error_code::texture_sampler_creation_failed);
    EXPECT_EQ(texture.generation, 7u);
    EXPECT_EQ(texture.m_internal_data, reinterpret_cast<void*>(0x1));
    EXPECT_EQ(renderer.destroyed_textures(), 0u);

    renderer.fail_texture_create(false);
    auto loaded = renderer.load_texture("cobblestone", texture);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(texture.width, 512u);
    EXPECT_EQ(texture.height, 512u);
    EXPECT_EQ(texture.channel_count, 4u);
    EXPECT_EQ(texture.generation, 8u);
    EXPECT_EQ(texture.m_internal_data, reinterpret_cast<void*>(0x2));
    EXPECT_EQ(renderer.destroyed_textures(), 1u);
}

TEST(RendererResult, KeepsTextureStateWhenImageLoadingFails) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};
    nk::Texture texture{
        .generation = 3,
        .m_internal_data = reinterpret_cast<void*>(0x1),
    };

    auto missing = renderer.load_texture("missing", texture);
    ASSERT_FALSE(missing);
    EXPECT_EQ(
        missing.error().code,
        nk::renderer_error_code::texture_file_failed);
    EXPECT_EQ(texture.generation, 3u);
    EXPECT_EQ(texture.m_internal_data, reinterpret_cast<void*>(0x1));
    EXPECT_EQ(renderer.destroyed_textures(), 0u);
}

TEST(RendererResult, CyclesChapterTexturesAndAdvancesGeneration) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    TestRenderer renderer{allocator, TestRenderer::BeginMode::render};

    ASSERT_TRUE(renderer.cycle_debug_texture());
    EXPECT_EQ(renderer.diffuse_texture().width, 512u);
    EXPECT_EQ(renderer.diffuse_texture().height, 512u);
    EXPECT_EQ(renderer.diffuse_texture().generation, 0u);

    ASSERT_TRUE(renderer.cycle_debug_texture());
    EXPECT_EQ(renderer.diffuse_texture().width, 480u);
    EXPECT_EQ(renderer.diffuse_texture().height, 480u);
    EXPECT_EQ(renderer.diffuse_texture().generation, 1u);

    ASSERT_TRUE(renderer.cycle_debug_texture());
    EXPECT_EQ(renderer.diffuse_texture().width, 480u);
    EXPECT_EQ(renderer.diffuse_texture().height, 480u);
    EXPECT_EQ(renderer.diffuse_texture().generation, 2u);
    EXPECT_EQ(renderer.destroyed_textures(), 2u);

    ASSERT_TRUE(renderer.cycle_debug_texture());
    EXPECT_EQ(renderer.diffuse_texture().width, 512u);
    EXPECT_EQ(renderer.diffuse_texture().generation, 3u);
    EXPECT_EQ(renderer.destroyed_textures(), 3u);
}
