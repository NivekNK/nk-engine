#pragma once

#include "vulkan/vk.h"
#include "collections/dyarr.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

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
        struct CommandDispatch {
            PFN_vkCmdBeginRendering begin_rendering = nullptr;
            PFN_vkCmdEndRendering end_rendering = nullptr;
            PFN_vkCmdPipelineBarrier2 pipeline_barrier = nullptr;
            PFN_vkQueueSubmit2 queue_submit = nullptr;
        };
        Device() = default;
        ~Device() = default;

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        void init(Platform* platform, Instance* instance, mem::Allocator* allocator, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        SwapchainSupportInfo& query_swapchain_support_info();
        bool find_memory_index(
            const u32 type_filter,
            VkMemoryPropertyFlags property_flags,
            u32* out_memory_index) const;

        VkSurfaceKHR get_surface() { return m_surface; }
        const PhysicalDeviceQueueFamilyInfo& get_queue_family_info() const { return m_queue_family_info; }
        VkFormat get_depth_format() const { return m_depth_format; }
        VkCommandPool get_graphics_command_pool() { return m_graphics_command_pool; }
        VkQueue get_graphics_queue() { return m_graphics_queue; }
        VkQueue get_present_queue() { return m_present_queue; }
        bool supports_device_local_host_visible() const noexcept {
            return m_supports_device_local_host_visible;
        }
        u64 uniform_buffer_offset_alignment() const noexcept {
            const u64 alignment =
                m_properties.limits.minUniformBufferOffsetAlignment;
            return alignment == 0 ? 1 : alignment;
        }
        u32 max_push_constant_size() const noexcept {
            return m_properties.limits.maxPushConstantsSize;
        }
        f32 timestamp_period() const noexcept { return m_properties.limits.timestampPeriod; }
        u32 timestamp_valid_bits() const noexcept { return m_timestamp_valid_bits; }
        bool dynamic_rendering() const noexcept { return m_commands.begin_rendering != nullptr; }
        bool synchronization2() const noexcept { return m_commands.pipeline_barrier != nullptr; }
        const CommandDispatch& commands() const noexcept { return m_commands; }
        [[nodiscard]] result<void, renderer_error> submit(
            VkCommandBuffer commands, VkSemaphore acquired,
            VkSemaphore rendered, VkFence completion);
        mem::Allocator* allocator() const { return m_allocator; }

        VkDevice get() { return m_logical_device; }
        VkDevice operator()() { return m_logical_device; }
        operator VkDevice() { return m_logical_device; }

    private:
        bool select_physical_device();
        bool detect_depth_format();
        void create_logical_device();
        void obtain_queues();
        void create_command_pool();

        Instance* m_instance = nullptr;
        mem::Allocator* m_allocator = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;

        // Surface
        VkSurfaceKHR m_surface = nullptr;

        // Physical Device
        VkPhysicalDevice m_physical_device = nullptr;
        PhysicalDeviceQueueFamilyInfo m_queue_family_info{};
        VkPhysicalDeviceProperties m_properties{};
        VkPhysicalDeviceFeatures m_features{};
        VkPhysicalDeviceMemoryProperties m_memory{};
        u32 m_timestamp_valid_bits = 0;
        bool m_supports_device_local_host_visible = false;
        SwapchainSupportInfo m_swapchain_support_info{};

        // Depth format
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;

        // Logical Device
        VkDevice m_logical_device = nullptr;
        CommandDispatch m_commands{};

        // Queues
        VkQueue m_graphics_queue = nullptr;
        VkQueue m_present_queue = nullptr;
        VkQueue m_transfer_queue = nullptr;

        // Command pool
        VkCommandPool m_graphics_command_pool = nullptr;
    };
}
