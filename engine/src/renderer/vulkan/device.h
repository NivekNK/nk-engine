#pragma once

#include "vulkan/vk.h"
#include "collections/dyarr.h"

namespace nk {
    class Platform;
    class Instance;
    namespace mem {
        class Allocator;
    }

    struct PhysicalDeviceRequirements {
        bool graphics;
        bool present;
        bool compute;
        bool transfer;
        bool sampler_anisotropy;
        bool discrete_gpu;
        cl::dyarr<cstr> extensions;
    };

    struct PhysicalDeviceQueueFamilyInfo {
        u32 graphics_family_index;
        u32 present_family_index;
        u32 compute_family_index;
        u32 transfer_family_index;
    };

    struct SwapchainSupportInfo {
        VkSurfaceCapabilitiesKHR capabilities;
        cl::dyarr<VkSurfaceFormatKHR> formats;
        cl::dyarr<VkPresentModeKHR> present_modes;
    };

    class Device {
    public:
        Device() = default;
        ~Device() = default;

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        void init(Platform* platform, Instance* instance, mem::Allocator* allocator, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

    private:
        bool select_physical_device();
        bool detect_depth_format();
        void create_logical_device();
        void obtain_queues();
        void create_command_pool();

        Instance* m_instance;
        mem::Allocator* m_allocator;
        VkAllocationCallbacks* m_vulkan_allocator;

        // Surface
        VkSurfaceKHR m_surface;

        // Physical Device
        VkPhysicalDevice m_physical_device;
        PhysicalDeviceQueueFamilyInfo m_queue_family_info;
        VkPhysicalDeviceProperties m_properties;
        VkPhysicalDeviceFeatures m_features;
        VkPhysicalDeviceMemoryProperties m_memory;
        SwapchainSupportInfo m_swapchain_support_info;

        // Depth format
        VkFormat m_depth_format;

        // Logical Device
        VkDevice m_logical_device;

        // Queues
        VkQueue m_graphics_queue;
        VkQueue m_present_queue;
        VkQueue m_transfer_queue;

        // Command pool
        VkCommandPool m_graphics_command_pool;
    };
}
