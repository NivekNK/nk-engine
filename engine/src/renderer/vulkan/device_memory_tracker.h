#pragma once

#include "core/thread.h"
#include "vulkan/vk.h"

namespace nk::vk {
    struct DeviceMemoryStatistics {
        u64 active_allocations = 0;
        u64 allocated_bytes = 0;
        u64 peak_allocated_bytes = 0;
        u64 device_local_bytes = 0;
        u64 peak_device_local_bytes = 0;
        u64 host_visible_bytes = 0;
        u64 peak_host_visible_bytes = 0;
    };

    // Tracks VkDeviceMemory only. Vulkan implementation host allocations are
    // deliberately tracked by VulkanHostAllocator instead.
    class DeviceMemoryTracker final {
    public:
        DeviceMemoryTracker() noexcept;

        DeviceMemoryTracker(const DeviceMemoryTracker&) = delete;
        DeviceMemoryTracker& operator=(const DeviceMemoryTracker&) = delete;
        DeviceMemoryTracker(DeviceMemoryTracker&&) = delete;
        DeviceMemoryTracker& operator=(DeviceMemoryTracker&&) = delete;

        [[nodiscard]] bool record_allocation(
            u64 size,
            VkMemoryPropertyFlags properties) noexcept;
        [[nodiscard]] bool record_release(
            u64 size,
            VkMemoryPropertyFlags properties) noexcept;
        [[nodiscard]] DeviceMemoryStatistics statistics() const noexcept;
        [[nodiscard]] bool reset() noexcept;

    private:
        mutable Mutex m_mutex;
        DeviceMemoryStatistics m_statistics{};
    };
}
