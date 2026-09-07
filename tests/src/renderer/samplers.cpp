#include <gtest/gtest.h>
#include <bit>
#include "memory/malloc_allocator.h"
#include "vulkan/samplers.h"
#include "resources/texture.h"

namespace {
    using namespace nk;
    struct FakeDriver {
        u64 creates = 0, destroys = 0, waits = 0;
        VkResult creation = VK_SUCCESS, waiting = VK_SUCCESS;
        static FakeDriver* current;
        static VKAPI_ATTR VkResult VKAPI_CALL create(VkDevice, const VkSamplerCreateInfo*,
            const VkAllocationCallbacks*, VkSampler* output) {
            const u64 identity = ++current->creates;
            if (current->creation == VK_SUCCESS) *output = std::bit_cast<VkSampler>(identity);
            return current->creation;
        }
        static VKAPI_ATTR void VKAPI_CALL destroy(VkDevice, VkSampler, const VkAllocationCallbacks*) { ++current->destroys; }
        static VKAPI_ATTR VkResult VKAPI_CALL wait(VkDevice) { ++current->waits; return current->waiting; }
    };
    FakeDriver* FakeDriver::current = nullptr;
    class SamplerRegistry : public testing::Test {
    protected:
        mem::MallocAllocator allocator{mem::untracked};
        FakeDriver driver;
        vk::Samplers samplers;
        void SetUp() override {
            FakeDriver::current = &driver;
            ASSERT_TRUE(samplers.init(allocator, std::bit_cast<VkDevice>(u64{1}), nullptr,
                {true, 8, 2}, {FakeDriver::create, FakeDriver::destroy, FakeDriver::wait}));
        }
        void TearDown() override {
            samplers.shutdown();
            EXPECT_EQ(allocator.get_active_allocation_count(), 0);
            FakeDriver::current = nullptr;
        }
    };
}

TEST(Samplers, MapsDefaultsFiltersWrapAndDeviceLimits) {
    const SamplerConfig defaults;
    ASSERT_TRUE(defaults.valid());
    auto info = vk::sampler_create_info(defaults, {true, 8, 64});
    ASSERT_TRUE(info);
    EXPECT_EQ(info->minFilter, VK_FILTER_LINEAR);
    EXPECT_EQ(info->magFilter, VK_FILTER_LINEAR);
    EXPECT_EQ(info->mipmapMode, VK_SAMPLER_MIPMAP_MODE_LINEAR);
    EXPECT_EQ(info->addressModeU, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    EXPECT_EQ(info->maxAnisotropy, 8);
    EXPECT_EQ(info->anisotropyEnable, VK_TRUE);
    EXPECT_EQ(info->maxLod, VK_LOD_CLAMP_NONE);
    EXPECT_EQ(info->borderColor, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK);
    auto unsupported = vk::sampler_create_info(defaults, {false, 0, 64});
    ASSERT_TRUE(unsupported);
    EXPECT_EQ(unsupported->anisotropyEnable, VK_FALSE);
    EXPECT_EQ(unsupported->maxAnisotropy, 1);
    constexpr VkSamplerAddressMode expected[]{VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER};
    for (u8 wrap = 0; wrap < 4; ++wrap) {
        SamplerConfig config{TextureFilter::nearest, TextureFilter::nearest, TextureFilter::nearest,
            static_cast<TextureWrap>(wrap), static_cast<TextureWrap>(wrap), static_cast<TextureWrap>(wrap), 1};
        auto mapped = vk::sampler_create_info(config, {true, 16, 64});
        ASSERT_TRUE(mapped);
        EXPECT_EQ(mapped->minFilter, VK_FILTER_NEAREST);
        EXPECT_EQ(mapped->magFilter, VK_FILTER_NEAREST);
        EXPECT_EQ(mapped->mipmapMode, VK_SAMPLER_MIPMAP_MODE_NEAREST);
        EXPECT_EQ(mapped->addressModeU, expected[wrap]);
        EXPECT_EQ(mapped->addressModeV, expected[wrap]);
        EXPECT_EQ(mapped->addressModeW, expected[wrap]);
        EXPECT_EQ(mapped->anisotropyEnable, VK_FALSE);
    }
}

TEST(Samplers, RejectsInvalidEnumsAndNonFiniteOrSubunitAnisotropy) {
    SamplerConfig invalid;
    invalid.min_filter = static_cast<TextureFilter>(2);
    EXPECT_FALSE(vk::sampler_create_info(invalid, {true, 16, 64}));
    invalid = {};
    invalid.wrap_w = static_cast<TextureWrap>(4);
    EXPECT_FALSE(vk::sampler_create_info(invalid, {false, 1, 64}));
    invalid = {};
    for (f32 value : {0.0f, -1.0f, std::numeric_limits<f32>::infinity(), std::numeric_limits<f32>::quiet_NaN()}) {
        invalid.anisotropy = value;
        EXPECT_FALSE(vk::sampler_create_info(invalid, {true, 16, 64}));
    }
}

TEST(Samplers, DescriptorIdentityIncludesTextureAndSamplerGeneration) {
    Texture first, other_default;
    const vk::SampledImageKey old{&first, 0, {0, 0}};
    EXPECT_EQ(old, (vk::SampledImageKey{&first, 0, {0, 0}}));
    EXPECT_NE(old, (vk::SampledImageKey{&first, 1, {0, 0}}));
    EXPECT_NE(old, (vk::SampledImageKey{&first, 0, {1, 0}}));
    EXPECT_NE(old, (vk::SampledImageKey{&first, 0, {0, 1}}));
    EXPECT_NE(old, (vk::SampledImageKey{&other_default, 0, {0, 0}}));
}

TEST_F(SamplerRegistry, BoundsAllocationsAndRejectsStaleHandlesOnSlotReuse) {
    auto first = samplers.create({}), second = samplers.create({});
    ASSERT_TRUE(first); ASSERT_TRUE(second);
    EXPECT_NE(*first, *second);
    EXPECT_EQ(samplers.live_count(), 2);
    auto full = samplers.create({});
    ASSERT_FALSE(full);
    EXPECT_EQ(full.error().code, renderer_error_code::sampler_capacity_exceeded);
    EXPECT_EQ(driver.creates, 2);
    ASSERT_TRUE(samplers.release(*first));
    EXPECT_EQ(driver.waits, 1);
    EXPECT_EQ(driver.destroys, 1);
    EXPECT_EQ(samplers.resolve(*first), VK_NULL_HANDLE);
    auto replacement = samplers.create({});
    ASSERT_TRUE(replacement);
    EXPECT_EQ(replacement->index, first->index);
    EXPECT_NE(replacement->generation, first->generation);
    EXPECT_FALSE(samplers.release(*first));
    EXPECT_NE(samplers.resolve(*replacement), VK_NULL_HANDLE);
    EXPECT_NE(samplers.resolve(*second), VK_NULL_HANDLE);
}

TEST_F(SamplerRegistry, PreservesRegistryOnCreateAndWaitFailures) {
    driver.creation = VK_ERROR_OUT_OF_HOST_MEMORY;
    EXPECT_FALSE(samplers.create({}));
    EXPECT_EQ(samplers.live_count(), 0);
    driver.creation = VK_SUCCESS;
    auto handle = samplers.create({}); ASSERT_TRUE(handle);
    driver.waiting = VK_ERROR_DEVICE_LOST;
    auto failed = samplers.release(*handle);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code, renderer_error_code::device_wait_failed);
    EXPECT_NE(samplers.resolve(*handle), VK_NULL_HANDLE);
    EXPECT_EQ(driver.destroys, 0);
    driver.waiting = VK_SUCCESS;
    EXPECT_TRUE(samplers.release(*handle));
    EXPECT_EQ(samplers.live_count(), 0);
}
