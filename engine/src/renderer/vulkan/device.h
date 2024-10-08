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

        bool find_memory_index(
            const u32 type_filter,
            VkMemoryPropertyFlags property_flags,
            u32* out_memory_index) const;

        const SwapchainSupportInfo& query_swapchain_support_info() { return query_swapchain_support_info(m_physical_device); }
        const SwapchainSupportInfo& get_swapchain_support_info() { return m_swapchain_support_info; }
        VkSurfaceKHR get_surface() { return m_surface; }
        const PhysicalDeviceQueueFamilyInfo& get_queue_family_info() const { return m_queue_family_info; }
        const VkFormat get_depth_format() const { return m_depth_format; }
        VkCommandPool get_graphics_command_pool() { return m_graphics_command_pool; }
        VkQueue get_graphics_queue() { return m_graphics_queue; }
        VkQueue get_present_queue() { return m_present_queue; }

        VkDevice get() { return m_logical_device; }
        VkDevice operator()() { return m_logical_device; }
        operator VkDevice() { return m_logical_device; }

    private:
        bool select_physical_device();
        bool detect_depth_format();
        void create_logical_device();
        void obtain_queues();
        void create_command_pool();

        const SwapchainSupportInfo& query_swapchain_support_info(VkPhysicalDevice physical_device);

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
        PhysicalDeviceQueueFamilyInfo m_queue_family_info;
        VkPhysicalDeviceProperties m_properties;
        VkPhysicalDeviceFeatures m_features;
        VkPhysicalDeviceMemoryProperties m_memory;

        VkFormat m_depth_format;

        VkDevice m_logical_device;
        VkQueue m_graphics_queue;
        VkQueue m_present_queue;
        VkQueue m_transfer_queue;

        VkCommandPool m_graphics_command_pool;
    };
}
