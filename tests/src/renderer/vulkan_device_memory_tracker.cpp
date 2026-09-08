#include <gtest/gtest.h>

#include "renderer/vulkan/device_memory_tracker.h"

TEST(VulkanDeviceMemoryTracker, SeparatesDeviceAndHostVisibleMemory) {
    nk::vk::DeviceMemoryTracker tracker;

    ASSERT_TRUE(tracker.record_allocation(
        1024, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    ASSERT_TRUE(tracker.record_allocation(
        256,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    const auto active = tracker.statistics();
    EXPECT_EQ(active.active_allocations, 2);
    EXPECT_EQ(active.allocated_bytes, 1280);
    EXPECT_EQ(active.device_local_bytes, 1280);
    EXPECT_EQ(active.host_visible_bytes, 256);

    ASSERT_TRUE(tracker.record_release(
        1024, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    ASSERT_TRUE(tracker.record_release(
        256,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    const auto released = tracker.statistics();
    EXPECT_EQ(released.active_allocations, 0);
    EXPECT_EQ(released.allocated_bytes, 0);
    EXPECT_EQ(released.peak_allocated_bytes, 1280);
    EXPECT_EQ(released.peak_device_local_bytes, 1280);
    EXPECT_EQ(released.peak_host_visible_bytes, 256);
}

TEST(VulkanDeviceMemoryTracker, RejectsFailedAndMismatchedEvents) {
    nk::vk::DeviceMemoryTracker tracker;

    EXPECT_FALSE(tracker.record_allocation(0, 0));
    EXPECT_FALSE(tracker.record_release(64, 0));
    ASSERT_TRUE(tracker.record_allocation(
        32, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    EXPECT_FALSE(tracker.record_release(
        64, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    auto unchanged = tracker.statistics();
    EXPECT_EQ(unchanged.active_allocations, 1);
    EXPECT_EQ(unchanged.allocated_bytes, 32);
    EXPECT_FALSE(tracker.reset());

    ASSERT_TRUE(tracker.record_release(
        32, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    ASSERT_TRUE(tracker.reset());
    EXPECT_EQ(tracker.statistics().peak_allocated_bytes, 0);
}
