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

    result<void, renderer_error> Fence::init(
        bool is_signaled,
        Device* device,
        VkAllocationCallbacks* vulkan_allocator) {
        m_is_signaled = is_signaled;
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (m_is_signaled) {
            fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }
        const VkResult result = vkCreateFence(
            m_device->get(),
            &fence_create_info,
            m_vulkan_allocator,
            &m_fence);
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::initialization_failed,
                .native_code = static_cast<i32>(result),
            });
        return ok();
    }

    void Fence::shutdown() {
        m_is_signaled = false;

        if (m_fence == nullptr)
            return;

        vkDestroyFence(m_device->get(), m_fence, m_vulkan_allocator);
        m_fence = nullptr;
    }

    result<void, renderer_error> Fence::wait(u64 timeout_ns) {
        if (m_is_signaled)
            return ok();

        VkResult result = vkWaitForFences(m_device->get(), 1, &m_fence, true, timeout_ns);
        switch (result) {
            case VK_SUCCESS:
                m_is_signaled = true;
                return ok();
            default:
                return err(renderer_error{
                    .code = renderer_error_code::fence_wait_failed,
                    .native_code = static_cast<i32>(result),
                });
        }
    }

    result<void, renderer_error> Fence::reset() {
        if (m_is_signaled) {
            const VkResult result = vkResetFences(m_device->get(), 1, &m_fence);
            if (result != VK_SUCCESS)
                return err(renderer_error{
                    .code = renderer_error_code::fence_reset_failed,
                    .native_code = static_cast<i32>(result),
                });
            m_is_signaled = false;
        }
        return ok();
    }
}
