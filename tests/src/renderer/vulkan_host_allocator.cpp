#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"
#include "memory/synchronized_allocator.h"
#include "renderer/vulkan/host_allocator.h"

namespace {
    struct CallbackFixture {
        nk::mem::MallocAllocator backing{nk::mem::untracked};
        nk::mem::SynchronizedAllocator synchronized{
            nk::mem::untracked,
            backing};
        nk::vk::VulkanHostAllocator allocator;

        CallbackFixture() {
            if (!allocator.init(synchronized))
                std::abort();
        }
    };
}

TEST(VulkanHostAllocator, AlignsTracksAndReleasesCallbackAllocations) {
    CallbackFixture fixture;
    VkAllocationCallbacks* callbacks = fixture.allocator.callbacks();
    ASSERT_NE(callbacks, nullptr);

    for (std::size_t alignment = 1; alignment <= 4096; alignment <<= 1) {
        void* allocation = callbacks->pfnAllocation(
            callbacks->pUserData,
            37,
            alignment,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        ASSERT_NE(allocation, nullptr) << "alignment=" << alignment;
        EXPECT_EQ(
            reinterpret_cast<std::uintptr_t>(allocation) % alignment,
            0u);
        std::memset(allocation, 0x5a, 37);
        callbacks->pfnFree(callbacks->pUserData, allocation);
    }

    const auto statistics = fixture.allocator.statistics();
    EXPECT_EQ(statistics.active_allocations, 0u);
    EXPECT_EQ(statistics.allocated_bytes, 0u);
    EXPECT_GE(statistics.peak_allocated_bytes, 37u);
    EXPECT_TRUE(fixture.allocator.shutdown());
    EXPECT_EQ(fixture.allocator.callbacks(), nullptr);
}

TEST(VulkanHostAllocator, ReallocatesTransactionallyAndPreservesBytes) {
    CallbackFixture fixture;
    VkAllocationCallbacks* callbacks = fixture.allocator.callbacks();
    auto* original = static_cast<nk::u8*>(callbacks->pfnAllocation(
        callbacks->pUserData,
        16,
        64,
        VK_SYSTEM_ALLOCATION_SCOPE_CACHE));
    ASSERT_NE(original, nullptr);
    for (nk::u8 index = 0; index < 16; ++index)
        original[index] = index;

    void* rejected = callbacks->pfnReallocation(
        callbacks->pUserData,
        original,
        std::numeric_limits<std::size_t>::max(),
        64,
        VK_SYSTEM_ALLOCATION_SCOPE_CACHE);
    EXPECT_EQ(rejected, nullptr);
    for (nk::u8 index = 0; index < 16; ++index)
        EXPECT_EQ(original[index], index);

    auto* grown = static_cast<nk::u8*>(callbacks->pfnReallocation(
        callbacks->pUserData,
        original,
        64,
        256,
        VK_SYSTEM_ALLOCATION_SCOPE_CACHE));
    ASSERT_NE(grown, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(grown) % 256, 0u);
    for (nk::u8 index = 0; index < 16; ++index)
        EXPECT_EQ(grown[index], index);

    auto* shrunk = static_cast<nk::u8*>(callbacks->pfnReallocation(
        callbacks->pUserData,
        grown,
        8,
        16,
        VK_SYSTEM_ALLOCATION_SCOPE_CACHE));
    ASSERT_NE(shrunk, nullptr);
    for (nk::u8 index = 0; index < 8; ++index)
        EXPECT_EQ(shrunk[index], index);
    callbacks->pfnFree(callbacks->pUserData, shrunk);
    EXPECT_EQ(fixture.allocator.statistics().active_allocations, 0u);
}

TEST(VulkanHostAllocator, TracksInternalAllocationNotificationsSeparately) {
    CallbackFixture fixture;
    VkAllocationCallbacks* callbacks = fixture.allocator.callbacks();
    callbacks->pfnInternalAllocation(
        callbacks->pUserData,
        128,
        VK_INTERNAL_ALLOCATION_TYPE_EXECUTABLE,
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
    auto statistics = fixture.allocator.statistics();
    EXPECT_EQ(statistics.internal_allocations, 1u);
    EXPECT_EQ(statistics.internal_bytes, 128u);
    EXPECT_FALSE(fixture.allocator.shutdown());

    callbacks->pfnInternalFree(
        callbacks->pUserData,
        128,
        VK_INTERNAL_ALLOCATION_TYPE_EXECUTABLE,
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
    statistics = fixture.allocator.statistics();
    EXPECT_EQ(statistics.internal_allocations, 0u);
    EXPECT_EQ(statistics.internal_bytes, 0u);
    EXPECT_TRUE(fixture.allocator.shutdown());
}
