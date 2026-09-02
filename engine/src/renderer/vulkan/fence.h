#pragma once

#include "vulkan/vk.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

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

        [[nodiscard]] result<void, renderer_error> init(
            bool is_signaled,
            Device* device,
            VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        [[nodiscard]] result<void, renderer_error> renew(
            bool is_signaled,
            Device* device,
            VkAllocationCallbacks* vulkan_allocator) {
            shutdown();
            return init(is_signaled, device, vulkan_allocator);
        }

        [[nodiscard]] result<void, renderer_error> wait(u64 timeout_ns);
        [[nodiscard]] result<void, renderer_error> reset();

        VkFence get() { return m_fence; }
        VkFence operator()() { return m_fence; }
        operator VkFence() { return m_fence; }

    private:
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        Device* m_device = nullptr;

        VkFence m_fence = nullptr;
        bool m_is_signaled = false;
    };
}
