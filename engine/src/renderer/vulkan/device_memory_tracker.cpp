#include "nkpch.h"

#include "vulkan/device_memory_tracker.h"

namespace nk::vk {
    namespace {
        [[nodiscard]] bool can_add(const u64 left, const u64 right) noexcept {
            return right <= numeric::u64_max - left;
        }
    }

    DeviceMemoryTracker::DeviceMemoryTracker() noexcept {
        (void)m_mutex.init();
    }

    bool DeviceMemoryTracker::record_allocation(
        const u64 size,
        const VkMemoryPropertyFlags properties) noexcept {
        if (size == 0)
            return false;
        auto lock = LockGuard::acquire(m_mutex);
        if (!lock || m_statistics.active_allocations == numeric::u64_max ||
            !can_add(m_statistics.allocated_bytes, size) ||
            ((properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 &&
             !can_add(m_statistics.device_local_bytes, size)) ||
            ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
             !can_add(m_statistics.host_visible_bytes, size))) {
            return false;
        }

        ++m_statistics.active_allocations;
        m_statistics.allocated_bytes += size;
        if ((properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)
            m_statistics.device_local_bytes += size;
        if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
            m_statistics.host_visible_bytes += size;

        if (m_statistics.allocated_bytes > m_statistics.peak_allocated_bytes)
            m_statistics.peak_allocated_bytes = m_statistics.allocated_bytes;
        if (m_statistics.device_local_bytes >
            m_statistics.peak_device_local_bytes) {
            m_statistics.peak_device_local_bytes =
                m_statistics.device_local_bytes;
        }
        if (m_statistics.host_visible_bytes >
            m_statistics.peak_host_visible_bytes) {
            m_statistics.peak_host_visible_bytes =
                m_statistics.host_visible_bytes;
        }
        return true;
    }

    bool DeviceMemoryTracker::record_release(
        const u64 size,
        const VkMemoryPropertyFlags properties) noexcept {
        if (size == 0)
            return false;
        auto lock = LockGuard::acquire(m_mutex);
        if (!lock || m_statistics.active_allocations == 0 ||
            size > m_statistics.allocated_bytes ||
            ((properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 &&
             size > m_statistics.device_local_bytes) ||
            ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
             size > m_statistics.host_visible_bytes)) {
            return false;
        }

        --m_statistics.active_allocations;
        m_statistics.allocated_bytes -= size;
        if ((properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0)
            m_statistics.device_local_bytes -= size;
        if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
            m_statistics.host_visible_bytes -= size;
        return true;
    }

    DeviceMemoryStatistics DeviceMemoryTracker::statistics() const noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        return lock ? m_statistics : DeviceMemoryStatistics{};
    }

    bool DeviceMemoryTracker::reset() noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        if (!lock || m_statistics.active_allocations != 0)
            return false;
        m_statistics = {};
        return true;
    }
}
