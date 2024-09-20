#pragma once

#include "vulkan/vk.h"
#include "core/dyarr.h"

namespace nk {
    class Window;
    class Instance;

    struct PhysicalDeviceRequirements {
        bool graphics;
        bool present;
        bool compute;
        bool transfer;
        bool sampler_anisotropy;
        bool discrete_gpu;
        Dyarr<cstr> extensions;
    };

    struct PhysicalDeviceQueueFamilyInfo {
        u32 graphics_family_index;
        u32 present_family_index;
        u32 compute_family_index;
        u32 transfer_family_index;
    };

    struct SwapchainSupportInfo {
        VkSurfaceCapabilitiesKHR capabilities;
        Dyarr<VkSurfaceFormatKHR> formats;
        Dyarr<VkPresentModeKHR> present_modes;
    };

    class Device {
    public:
        Device() = default;

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        void init(Window& window, Instance* instance, Allocator* allocator, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        const SwapchainSupportInfo& query_swapchain_support() { return query_swapchain_support(m_physical_device); }

    private:
        bool select_physical_device();

        const SwapchainSupportInfo& query_swapchain_support(VkPhysicalDevice physical_device);

        bool physical_device_meets_requirements(
            PhysicalDeviceQueueFamilyInfo* out_queue_family,
            VkPhysicalDevice physical_device,
            const VkPhysicalDeviceProperties& properties,
            const VkPhysicalDeviceFeatures& features,
            const PhysicalDeviceRequirements& requirements);

        Instance* m_instance;
        Allocator* m_allocator;
        VkAllocationCallbacks* m_vulkan_allocator;

        VkSurfaceKHR m_surface;
        SwapchainSupportInfo m_swapchain_support_info;

        VkPhysicalDevice m_physical_device;
        PhysicalDeviceQueueFamilyInfo m_queue_family;
        VkPhysicalDeviceProperties m_properties;
        VkPhysicalDeviceFeatures m_features;
        VkPhysicalDeviceMemoryProperties m_memory;
    };
}
