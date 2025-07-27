#pragma once

#include "vulkan/vk.h"

namespace nk {
    class Device;

    class Fence {
    public:
        Fence() = default;
        ~Fence() { shutdown(); }

        Fence(const Fence&) = delete;
        Fence& operator=(const Fence&) = delete;

        Fence(Fence&& other);
        Fence& operator=(Fence&& other);

        void init(bool is_signaled, Device* device, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        void renew(bool is_signaled, Device* device, VkAllocationCallbacks* vulkan_allocator) {
            shutdown();
            init(is_signaled, device, vulkan_allocator);
        }

        bool wait(u64 timeout_ns);
        void reset();

        VkFence get() { return m_fence; }
        VkFence operator()() { return m_fence; }
        operator VkFence() { return m_fence; }

    private:
        VkAllocationCallbacks* m_vulkan_allocator;
        Device* m_device;

        VkFence m_fence;
        bool m_is_signaled;
    };
}
