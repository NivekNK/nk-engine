#pragma once

#include "vulkan/vk.h"
#include "core/arr.h"
#include "core/dyarr.h"
#include <memory>

namespace nk {
    class Instance;
    class Window;

    struct PhysicalDeviceQueueFamilyInfo {
        u32 graphics_family_index;
        u32 present_family_index;
        u32 compute_family_index;
        u32 transfer_family_index;
    };

    struct PhysicalDeviceRequirements {
        bool graphics;
        bool present;
        bool compute;
        bool transfer;
        bool sampler_anisotropy;
        bool discrete_gpu;
        Dyarr<cstr> extensions;
    };

    struct SwapchainSupportInfo {
        VkSurfaceCapabilitiesKHR capabilities;
        Arr<VkSurfaceFormatKHR> formats;
        Arr<VkPresentModeKHR> present_modes;
    };

    class Device {
    public:
        Device() = default;

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;

        Device(Device&& other);
        Device& operator=(Device&& other);

        void init(Window& window, Instance& instance, Allocator* allocator, VkAllocationCallbacks* vulkan_allocator);
        void shutdown(Instance& instance);

        bool find_memory_index(
            u32& out_memory_index,
            const u32 type_filter,
            VkMemoryPropertyFlags property_flags) const;

        void detect_depth_format();

        const SwapchainSupportInfo& get_swapchain_support_info() const { return m_swapchain_support_info; }
        VkSurfaceKHR get_surface() { return m_surface; }
        const PhysicalDeviceQueueFamilyInfo& get_queue_family_info() const { return m_queue_family; }
        const VkFormat get_depth_format() const { return m_depth_format; }
        VkCommandPool get_graphics_command_pool() { return m_graphics_command_pool; }
        VkQueue get_graphics_queue() const { return m_graphics_queue; }
        VkQueue get_present_queue() const { return m_present_queue; }

        VkDevice get() { return m_logical_device; }
        VkDevice operator()() { return m_logical_device; }
        operator VkDevice() { return m_logical_device; }

    private:
        void select_physical_device(Instance& instance, Allocator* allocator);
        void create_logical_device();
        void create_command_pool();

        void query_swapchain_support(Allocator* allocator, VkPhysicalDevice physical_device);

        bool physical_device_meets_requirements(
            PhysicalDeviceQueueFamilyInfo* out_queue_family,
            VkPhysicalDevice physical_device,
            const VkPhysicalDeviceProperties& properties,
            const VkPhysicalDeviceFeatures& features,
            const PhysicalDeviceRequirements& requirementst,
            Allocator* allocator);

        VkAllocationCallbacks* m_allocator;

        VkSurfaceKHR m_surface;

        VkPhysicalDevice m_physical_device;
        PhysicalDeviceQueueFamilyInfo m_queue_family;
        VkPhysicalDeviceProperties m_properties;
        VkPhysicalDeviceFeatures m_features;
        VkPhysicalDeviceMemoryProperties m_memory;

        SwapchainSupportInfo m_swapchain_support_info;

        VkFormat m_depth_format;

        VkDevice m_logical_device;
        VkQueue m_graphics_queue;
        VkQueue m_present_queue;
        VkQueue m_transfer_queue;

        VkCommandPool m_graphics_command_pool;
    };
}