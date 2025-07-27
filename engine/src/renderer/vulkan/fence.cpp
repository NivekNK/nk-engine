#include "nkpch.h"

#include "vulkan/fence.h"

#include "vulkan/device.h"

namespace nk {
    Fence::Fence(Fence&& other)
        : m_vulkan_allocator{other.m_vulkan_allocator},
          m_device{other.m_device},
          m_fence{other.m_fence},
          m_is_signaled{other.m_is_signaled} {
        other.m_vulkan_allocator = nullptr;
        other.m_device = nullptr;
        other.m_fence = nullptr;
        other.m_is_signaled = false;
    }

    Fence& Fence::operator=(Fence&& other) {
        m_vulkan_allocator = other.m_vulkan_allocator;
        m_device = other.m_device;
        m_fence = other.m_fence;
        m_is_signaled = other.m_is_signaled;

        other.m_vulkan_allocator = nullptr;
        other.m_device = nullptr;
        other.m_fence = nullptr;
        other.m_is_signaled = false;

        return *this;
    }

    void Fence::init(bool is_signaled, Device* device, VkAllocationCallbacks* vulkan_allocator) {
        m_is_signaled = is_signaled;
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (m_is_signaled) {
            fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }
        VulkanCheck(vkCreateFence(m_device->get(), &fence_create_info, m_vulkan_allocator, &m_fence));
    }

    void Fence::shutdown() {
        m_is_signaled = false;

        if (m_fence == nullptr)
            return;

        vkDestroyFence(m_device->get(), m_fence, m_vulkan_allocator);
        m_fence = nullptr;
    }

    bool Fence::wait(u64 timeout_ns) {
        if (m_is_signaled)
            return true;

        VkResult result = vkWaitForFences(m_device->get(), 1, &m_fence, true, timeout_ns);
        switch (result) {
            case VK_SUCCESS:
                m_is_signaled = true;
                return true;
            case VK_TIMEOUT:
                WarnLog("Fence::wait - Timed out");
                break;
            case VK_ERROR_DEVICE_LOST:
                WarnLog("Fence::wait - VK_ERROR_DEVICE_LOST.");
                break;
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                WarnLog("Fence::wait - VK_ERROR_OUT_OF_HOST_MEMORY.");
                break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                WarnLog("Fence::wait - VK_ERROR_OUT_OF_DEVICE_MEMORY.");
                break;
            default:
                WarnLog("Fence::wait - An unknown error has occurred.");
                break;
        }

        return false;
    }

    void Fence::reset() {
        if (m_is_signaled) {
            VulkanCheck(vkResetFences(m_device->get(), 1, &m_fence));
            m_is_signaled = false;
        }
    }
}
