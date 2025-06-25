#include "nkpch.h"

#include "vulkan/device.h"

#include "vulkan/utils.h"
#include "vulkan/instance.h"
#include "memory/allocator.h"

namespace nk {
    bool physical_device_meets_requirements(
        PhysicalDeviceQueueFamilyInfo* out_queue_family,
        SwapchainSupportInfo* out_swapchain_support_info,
        mem::Allocator* allocator,
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        const VkPhysicalDeviceProperties& properties,
        const VkPhysicalDeviceFeatures& features,
        const PhysicalDeviceRequirements& requirements);

    void query_swapchain_support_info(
        SwapchainSupportInfo* out_swapchain_support_info,
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface);

    void Device::init(Platform* platform, Instance* instance, mem::Allocator* allocator, VkAllocationCallbacks* vulkan_allocator) {
        m_instance = instance;
        m_allocator = allocator;
        m_vulkan_allocator = vulkan_allocator;

        m_surface = vk::create_surface(platform, instance, vulkan_allocator);
        if (m_surface == nullptr)
            return;
        InfoLog("Vulkan surface created.");

        m_swapchain_support_info.formats.dyarr_init(m_allocator, 12);
        m_swapchain_support_info.present_modes.dyarr_init(m_allocator, 12);

        if (!select_physical_device())
            return;

        if (!detect_depth_format())
            return;

        create_logical_device();
        obtain_queues();
        create_command_pool();

        TraceLog("nk::Device initialized.");
    }

    void Device::shutdown() {
        vkDestroyCommandPool(m_logical_device, m_graphics_command_pool, m_vulkan_allocator);
        InfoLog("Vulkan Graphics Command Pool destroyed.");
        
        m_graphics_queue = nullptr;
        m_present_queue = nullptr;
        m_transfer_queue = nullptr;

        vkDestroyDevice(m_logical_device, m_vulkan_allocator);
        InfoLog("Vulkan Logical Device destroyed.");

        m_physical_device = nullptr;

        m_swapchain_support_info.formats.dyarr_shutdown();
        m_swapchain_support_info.present_modes.dyarr_shutdown();
        InfoLog("SwapchainSupportInfo freed.");

        if (m_surface != nullptr) {
            vkDestroySurfaceKHR(m_instance->get(), m_surface, m_vulkan_allocator);
            InfoLog("Vulkan Surface destroyed.");
        }

        m_vulkan_allocator = nullptr;
        m_allocator = nullptr;
        m_instance = nullptr;

        TraceLog("nk::Device shutdown.");
    }

    bool Device::select_physical_device() {
        u32 physical_device_count = 0;
        VulkanCheck(vkEnumeratePhysicalDevices(m_instance->get(), &physical_device_count, nullptr));
        if (physical_device_count == 0) {
            FatalLog("nk::Device::select_physical_device No devices which support Vulkan were found.");
            return false;
        }

        VkPhysicalDevice* physical_devices = m_allocator->allocate_lot_t(VkPhysicalDevice, physical_device_count);
        VulkanCheck(vkEnumeratePhysicalDevices(m_instance->get(), &physical_device_count, physical_devices));

        // TODO: These requirements should probably be driven by engine
        // configuration.
        PhysicalDeviceRequirements requirements = {
            .graphics = true,
            .present = true,
            // NOTE: Enable this if compute will be required.
            .compute = false,
            .transfer = true,
            .sampler_anisotropy = true,
            .discrete_gpu = true,
            .extensions = {},
        };
        requirements.extensions.dyarr_init(m_allocator, 1);
        requirements.extensions.dyarr_push_ptr(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        VkPhysicalDeviceMemoryProperties memory;

        bool found_physical_device = false;
        u64 selected_physical_device_index = 0;

        for (u64 i = 0; i < physical_device_count; i++) {
            vkGetPhysicalDeviceProperties(physical_devices[i], &properties);
            vkGetPhysicalDeviceFeatures(physical_devices[i], &features);
            vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &memory);

            bool meet_requirements = physical_device_meets_requirements(
                &m_queue_family_info,
                &m_swapchain_support_info,
                m_allocator,
                physical_devices[i],
                m_surface,
                properties,
                features,
                requirements);

            if (meet_requirements) {
                found_physical_device = true;
                selected_physical_device_index = i;
                break;
            }
        }

        requirements.extensions.dyarr_shutdown();

        if (!found_physical_device) {
            ErrorLog("nk::Device::select_physical_device No physical devices were found which meet the requirements.");
            m_allocator->free_lot_t(VkPhysicalDevice, physical_devices, physical_device_count);
            m_swapchain_support_info.formats.dyarr_shutdown();
            m_swapchain_support_info.present_modes.dyarr_shutdown();
            return false;
        }

#if defined(NK_DEBUG)
        DebugLog("Selected device: '{}'.", properties.deviceName);
        // GPU type, etc.
        switch (properties.deviceType) {
            default:
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                DebugLog("GPU type is Unknown.");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                DebugLog("GPU type is Integrated.");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                DebugLog("GPU type is Discrete.");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                DebugLog("GPU type is Virtual.");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                DebugLog("GPU type is CPU.");
                break;
        }

        const u32 api_major = VK_API_VERSION_MAJOR(properties.driverVersion);
        const u32 api_minor = VK_API_VERSION_MINOR(properties.driverVersion);
        const u32 api_patch = VK_API_VERSION_PATCH(properties.driverVersion);
        DebugLog("GPU Driver version: {}.{}.{}.", api_major, api_minor, api_patch);

        const u32 vk_api_major = VK_API_VERSION_MAJOR(properties.apiVersion);
        const u32 vk_api_minor = VK_API_VERSION_MINOR(properties.apiVersion);
        const u32 vk_api_patch = VK_API_VERSION_PATCH(properties.apiVersion);
        DebugLog("Vulkan API version: {}.{}.{}.", vk_api_major, vk_api_minor, vk_api_patch);

        // Memory information
        for (u32 i = 0; i < memory.memoryHeapCount; i++) {
            const f32 memory_size_gib = static_cast<f32>(memory.memoryHeaps[i].size) / 1024.0f / 1024.0f / 1024.0f;
            if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                DebugLog("[Memory Heap {}] Local GPU memory: {:.2f} GiB.", i, memory_size_gib);
            } else {
                DebugLog("[Memory Heap {}] Shared System memory: {:.2f} GiB.", i, memory_size_gib);
            }
        }

        DebugLog("Swapchain Support Info: [Formats: {}] [Preset Modes: {}]",
                 m_swapchain_support_info.formats.length(),
                 m_swapchain_support_info.present_modes.length());
#endif

        m_physical_device = physical_devices[selected_physical_device_index];
        m_properties = properties;
        m_features = features;
        m_memory = memory;

        m_allocator->free_lot_t(VkPhysicalDevice, physical_devices, physical_device_count);

        InfoLog("Vulkan Physical Device Selected.");
        return true;
    }

    bool Device::detect_depth_format() {
        constexpr u64 candidate_count = 3;
        VkFormat candidates[candidate_count] = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        constexpr cstr candidates_cstr[candidate_count] = {
            "VK_FORMAT_D32_SFLOAT",
            "VK_FORMAT_D32_SFLOAT_S8_UINT",
            "VK_FORMAT_D24_UNORM_S8_UINT"};
    #define _NK_GET_CANDIDATE(i) candidates_cstr[i]
#else
    #define _NK_GET_CANDIDATE(i) "INFO ONLY IN DEBUG MODE"
#endif

        u32 flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        for (u64 i = 0; i < candidate_count; i++) {
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(m_physical_device, candidates[i], &properties);

            if ((properties.linearTilingFeatures & flags) == flags) {
                m_depth_format = candidates[i];
                InfoLog("Vulkan Depth Format detected. Using: {}.", _NK_GET_CANDIDATE(i));
                return true;
            } else if ((properties.optimalTilingFeatures & flags) == flags) {
                m_depth_format = candidates[i];
                InfoLog("Vulkan Depth Format detected. Using: {}.", _NK_GET_CANDIDATE(i));
                return true;
            }
        }

        m_depth_format = VK_FORMAT_UNDEFINED;
        ErrorLog("nk::Device::detect_depth_format Depth format not detected.");
        return false;
    }

    void Device::create_logical_device() {
        // NOTE: Do not create additional queues for shared indices.
        constexpr u32 unique_queue_family_count = 3;
        u32 unique_queue_families[unique_queue_family_count] = {
            m_queue_family_info.graphics_family_index,
            m_queue_family_info.present_family_index,
            m_queue_family_info.transfer_family_index};

        VkDeviceQueueCreateInfo queue_create_infos[unique_queue_family_count] = {};
        u32 i = 0;
        for (const u32 queue_family_index : unique_queue_families) {
            queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_infos[i].queueFamilyIndex = queue_family_index;
            queue_create_infos[i].queueCount = 1;

            // TODO: Enable this for a future enhacement.
            // if (indices[i] == m_queue_family_info.graphics_family_index)
            //    queue_create_infos[i].queueCount = 2;

            queue_create_infos[i].flags = 0;
            queue_create_infos[i].pNext = nullptr;

            f32 queue_priority = 1.0f;
            queue_create_infos[i].pQueuePriorities = &queue_priority;
            i++;
        }

        // Request device features.
        // TODO: should be configurable.
        VkPhysicalDeviceFeatures device_features = {};
        device_features.samplerAnisotropy = VK_TRUE; // Request anistrophy

        VkDeviceCreateInfo device_create_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device_create_info.queueCreateInfoCount = unique_queue_family_count;
        device_create_info.pQueueCreateInfos = queue_create_infos;
        device_create_info.pEnabledFeatures = &device_features;
        device_create_info.enabledExtensionCount = 1;
        cstr extension_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        device_create_info.ppEnabledExtensionNames = &extension_names;

        // Deprecated and ignored, so pass nothing.
        device_create_info.enabledLayerCount = 0;
        device_create_info.ppEnabledLayerNames = nullptr;

        // Create the device.
        VulkanCheck(vkCreateDevice(m_physical_device, &device_create_info, m_vulkan_allocator, &m_logical_device));
        InfoLog("Vulkan Logical Device created.");
    }

    void Device::obtain_queues() {
        vkGetDeviceQueue(m_logical_device, m_queue_family_info.graphics_family_index, 0, &m_graphics_queue);
        vkGetDeviceQueue(m_logical_device, m_queue_family_info.present_family_index, 0, &m_present_queue);
        vkGetDeviceQueue(m_logical_device, m_queue_family_info.transfer_family_index, 0, &m_transfer_queue);
        InfoLog("Vulkan Queues obtained.");
    }

    void Device::create_command_pool() {
        VkCommandPoolCreateInfo pool_create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool_create_info.queueFamilyIndex = m_queue_family_info.graphics_family_index;
        pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VulkanCheck(vkCreateCommandPool(m_logical_device, &pool_create_info, m_vulkan_allocator, &m_graphics_command_pool));
        InfoLog("Vulkan Graphics Command Pool created.");
    }

    bool physical_device_meets_requirements(
        PhysicalDeviceQueueFamilyInfo* out_queue_family,
        SwapchainSupportInfo* out_swapchain_support_info,
        mem::Allocator* allocator,
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface,
        const VkPhysicalDeviceProperties& properties,
        const VkPhysicalDeviceFeatures& features,
        const PhysicalDeviceRequirements& requirements) {
        // Evaluate device properties to determine if it meets the needs of our application.
        out_queue_family->graphics_family_index = -1;
        out_queue_family->present_family_index = -1;
        out_queue_family->compute_family_index = -1;
        out_queue_family->transfer_family_index = -1;

        // Check if discrete GPU
        if (requirements.discrete_gpu && properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            DebugLog("Device {} is not a discrete GPU, and one is required. Skipping.", properties.deviceName);
            return false;
        }

        // Sampler anisotropy
        if (requirements.sampler_anisotropy && !features.samplerAnisotropy) {
            DebugLog("Device {} does not support samplerAnisotropy, skipping.", properties.deviceName);
            return false;
        }

        u32 queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        VkQueueFamilyProperties* queue_families = allocator->allocate_lot_t(VkQueueFamilyProperties, queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

        // Look at each queue and see what queues it supports
        u8 min_transfer_score = 255;
        for (u32 i = 0; i < queue_family_count; i++) {
            u8 current_transfer_score = 0;

            // Graphics queue
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                out_queue_family->graphics_family_index = i;
                current_transfer_score++;
            }

            // Compute queue
            if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                out_queue_family->compute_family_index = i;
                current_transfer_score++;
            }

            // Transfer queue
            if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                // Take the index if it is the current lowest. This increases the
                // liklihood that it is a dedicated transfer queue.
                if (current_transfer_score <= min_transfer_score) {
                    min_transfer_score = current_transfer_score;
                    out_queue_family->transfer_family_index = i;
                }
            }

            // Present queue
            VkBool32 supports_present = VK_FALSE;
            VulkanCheck(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_present));
            if (supports_present) {
                out_queue_family->present_family_index = i;
            }
        }
        allocator->free_lot_t(VkQueueFamilyProperties, queue_families, queue_family_count);

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        str graphics_family_index_str = out_queue_family->graphics_family_index != -1 ? "true" : "false";
        str present_family_index_str = out_queue_family->present_family_index != -1 ? "true" : "false";
        str compute_family_index_str = out_queue_family->compute_family_index != -1 ? "true" : "false";
        str transfer_family_index_str = out_queue_family->transfer_family_index != -1 ? "true" : "false";
        DebugLog("Device selected:\n{:>12} | {:>12} | {:>12} | {:>12} | {:>12}\n{:>12} | {:>12} | {:>12} | {:>12} | {:>12}",
                 "Graphics", "Present", "Compute", "Transfer", "Name",
                 graphics_family_index_str,
                 present_family_index_str,
                 compute_family_index_str,
                 transfer_family_index_str,
                 properties.deviceName);
#endif

        if ((requirements.graphics && out_queue_family->graphics_family_index == -1) ||
            (requirements.present && out_queue_family->present_family_index == -1) ||
            (requirements.compute && out_queue_family->compute_family_index == -1) ||
            (requirements.transfer && out_queue_family->transfer_family_index == -1)) {
            return false;
        }

        DebugLog("Device meets queue requirements.");
        DebugLog("Graphics Family Index: {}.", out_queue_family->graphics_family_index);
        DebugLog("Present Family Index:  {}.", out_queue_family->present_family_index);
        DebugLog("Transfer Family Index: {}.", out_queue_family->transfer_family_index);
        DebugLog("Compute Family Index:  {}.", out_queue_family->compute_family_index);

        query_swapchain_support_info(out_swapchain_support_info, physical_device, surface);
        if (out_swapchain_support_info->formats.empty() || out_swapchain_support_info->present_modes.empty()) {
            out_swapchain_support_info->formats.reset();
            out_swapchain_support_info->present_modes.reset();
            DebugLog("Required swapchain support not present, skipping device {}.", properties.deviceName);
            return false;
        }

        u32 available_extension_count = 0;
        VulkanCheck(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extension_count, nullptr));
        VkExtensionProperties* available_extensions_properties = allocator->allocate_lot_t(VkExtensionProperties, available_extension_count);
        VulkanCheck(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extension_count, available_extensions_properties));

        for (u64 i = 0; i < requirements.extensions.length(); i++) {
            bool found_extension = false;

            for (u64 j = 0; j < available_extension_count; j++) {
                if (strcmp(available_extensions_properties[j].extensionName, requirements.extensions[i]) == 0) {
                    found_extension = true;
                    break;
                }
            }

            if (!found_extension) {
                DebugLog("Required extension not found: '{}', skipping device {}.", requirements.extensions[i], properties.deviceName);
                allocator->free_lot_t(VkExtensionProperties, available_extensions_properties, available_extension_count);
                out_swapchain_support_info->formats.reset();
                out_swapchain_support_info->present_modes.reset();
                return false;
            }
        }
        allocator->free_lot_t(VkExtensionProperties, available_extensions_properties, available_extension_count);

        return true;
    }

    void query_swapchain_support_info(
        SwapchainSupportInfo* out_swapchain_support_info,
        VkPhysicalDevice physical_device,
        VkSurfaceKHR surface) {
        // Surface capabilities
        VulkanCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &out_swapchain_support_info->capabilities));

        // Surface formats
        u32 format_count = 0;
        VulkanCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr));
        if (format_count > 0) {
            if (out_swapchain_support_info->formats.length() < format_count)
                out_swapchain_support_info->formats.dyarr_resize(format_count);

            VulkanCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(
                physical_device,
                surface,
                &format_count,
                out_swapchain_support_info->formats.data()));
        } else {
            out_swapchain_support_info->formats.reset();
        }

        // Present modes
        u32 present_mode_count = 0;
        VulkanCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr));
        if (present_mode_count > 0) {
            if (out_swapchain_support_info->present_modes.length() < present_mode_count)
                out_swapchain_support_info->present_modes.dyarr_resize(present_mode_count);

            VulkanCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(
                physical_device,
                surface,
                &present_mode_count,
                out_swapchain_support_info->present_modes.data()));
        } else {
            out_swapchain_support_info->present_modes.reset();
        }
    }
}
