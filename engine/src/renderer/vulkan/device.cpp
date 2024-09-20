#include "nkpch.h"

#include "vulkan/device.h"

#include "vulkan/utils.h"
#include "vulkan/instance.h"

namespace nk {
    void Device::init(Window& window, Instance* instance, Allocator* allocator, VkAllocationCallbacks* vulkan_allocator) {
        m_instance = instance;
        m_allocator = allocator;
        ;
        m_vulkan_allocator = vulkan_allocator;

        m_surface = vk::create_surface(window, m_instance, m_vulkan_allocator);
        if (m_surface == nullptr)
            return;
        InfoLog("Vulkan surface created.");

        if (!select_physical_device())
            return;

        TraceLog("nk::Device initialized.");
    }

    void Device::shutdown() {
        m_swapchain_support_info.formats.clear();
        m_swapchain_support_info.present_modes.clear();
        m_physical_device = nullptr;

        vkDestroySurfaceKHR(m_instance->get(), m_surface, m_vulkan_allocator);
        InfoLog("Vulkan Surface destroyed.");

        m_vulkan_allocator = nullptr;
        m_allocator = nullptr;
        m_instance = nullptr;

        TraceLog("nk::Device shutdown.");
    }

    bool Device::select_physical_device() {
        u32 physical_device_count = 0;
        VulkanCheck(vkEnumeratePhysicalDevices(m_instance->get(), &physical_device_count, 0));
        if (physical_device_count == 0) {
            FatalLog("Device::select_physical_device No devices which support Vulkan were found.");
            return false;
        }

        VkPhysicalDevice physical_devices[physical_device_count];
        VulkanCheck(vkEnumeratePhysicalDevices(m_instance->get(), &physical_device_count, physical_devices));

        // TODO: These requirements should probably be driven by engine
        // configuration.
        PhysicalDeviceRequirements requirements = {
            .graphics = true,
            .present = true,
            .transfer = true,
            // NOTE: Enable this if compute will be required.
            //.compute = true,
            .sampler_anisotropy = true,
            .discrete_gpu = true,
        };
        requirements.extensions.init_list(m_allocator, {VK_KHR_SWAPCHAIN_EXTENSION_NAME});

        for (u32 i = 0; i < physical_device_count; i++) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(physical_devices[i], &properties);

            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceFeatures(physical_devices[i], &features);

            VkPhysicalDeviceMemoryProperties memory;
            vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &memory);

            PhysicalDeviceQueueFamilyInfo queue_family;
            bool meet_requirements = physical_device_meets_requirements(
                &queue_family,
                physical_devices[i],
                properties,
                features,
                requirements);

            if (!meet_requirements) {
                continue;
            }

            requirements.extensions.clear();

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

            DebugLog("GPU Driver version: {}.{}.{}.",
                     VK_API_VERSION_MAJOR(properties.driverVersion),
                     VK_API_VERSION_MINOR(properties.driverVersion),
                     VK_API_VERSION_PATCH(properties.driverVersion));

            DebugLog("Vulkan API version: {}.{}.{}.",
                     VK_API_VERSION_MAJOR(properties.apiVersion),
                     VK_API_VERSION_MINOR(properties.apiVersion),
                     VK_API_VERSION_PATCH(properties.apiVersion));

            // Memory information
            for (u32 i = 0; i < memory.memoryHeapCount; i++) {
                const f32 memory_size_gib = static_cast<f32>(memory.memoryHeaps[i].size) / 1024.0f / 1024.0f / 1024.0f;
                if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    DebugLog("[Memory Heap {}] Local GPU memory: {:.2f} GiB.", i, memory_size_gib);
                } else {
                    DebugLog("[Memory Heap {}] Shared System memory: {:.2f} GiB.", i, memory_size_gib);
                }
            }
#endif

            m_physical_device = physical_devices[i];
            m_queue_family = queue_family;

            // Keep a copy of properties, features and memory info for later use.
            m_properties = properties;
            m_features = features;
            m_memory = memory;
        }

        if (m_physical_device == nullptr) {
            ErrorLog("Device::select_physical_device No physical devices were found which meet the requirements.");
            return false;
        }

        InfoLog("Vulkan Physical Device Selected.");
        return true;
    }

    const SwapchainSupportInfo& Device::query_swapchain_support(VkPhysicalDevice physical_device) {
        // Surface capabilities
        VulkanCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, m_surface, &m_swapchain_support_info.capabilities));

        // Surface formats
        u32 format_count = 0;
        VulkanCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, m_surface, &format_count, nullptr));
        if (format_count > 0) {
            if (m_swapchain_support_info.formats.capacity() > 0) {
                if (m_swapchain_support_info.formats.capacity() > format_count ||
                    m_swapchain_support_info.formats.capacity() < format_count) {
                    m_swapchain_support_info.formats.resize(format_count);
                }
            } else {
                m_swapchain_support_info.formats.init(m_allocator, format_count, format_count);
            }

            VulkanCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(
                physical_device,
                m_surface,
                &format_count,
                m_swapchain_support_info.formats.data()));
        } else {
            m_swapchain_support_info.formats.reset();
        }

        // Present modes
        u32 present_mode_count = 0;
        VulkanCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, m_surface, &present_mode_count, nullptr));
        if (present_mode_count > 0) {
            if (m_swapchain_support_info.present_modes.capacity() > 0) {
                if (m_swapchain_support_info.present_modes.capacity() > present_mode_count ||
                    m_swapchain_support_info.present_modes.capacity() < present_mode_count) {
                    m_swapchain_support_info.present_modes.resize(present_mode_count);
                }
            } else {
                m_swapchain_support_info.present_modes.init(m_allocator, present_mode_count, present_mode_count);
            }

            VulkanCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(
                physical_device,
                m_surface,
                &present_mode_count,
                m_swapchain_support_info.present_modes.data()));
        } else {
            m_swapchain_support_info.present_modes.reset();
        }

        return m_swapchain_support_info;
    }

    bool Device::physical_device_meets_requirements(
        PhysicalDeviceQueueFamilyInfo* out_queue_family,
        VkPhysicalDevice physical_device,
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
        VkQueueFamilyProperties* queue_families = m_allocator->allocate_lot(VkQueueFamilyProperties, queue_family_count);
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
            VulkanCheck(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, m_surface, &supports_present));
            if (supports_present) {
                out_queue_family->present_family_index = i;
            }
        }
        m_allocator->free_lot(VkQueueFamilyProperties, queue_families, queue_family_count);

#if defined(NK_DEBUG)
        DebugLog("Device selected:\n{:>12} | {:>12} | {:>12} | {:>12} | {:>12}\n{:>12} | {:>12} | {:>12} | {:>12} | {:>12}",
                 "Graphics", "Present", "Compute", "Transfer", "Name",
                 out_queue_family->graphics_family_index != -1 ? "true" : "false",
                 out_queue_family->present_family_index != -1 ? "true" : "false",
                 out_queue_family->compute_family_index != -1 ? "true" : "false",
                 out_queue_family->transfer_family_index != -1 ? "true" : "false",
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

        query_swapchain_support(physical_device);
        if (m_swapchain_support_info.formats.empty() || m_swapchain_support_info.present_modes.empty()) {
            m_swapchain_support_info.formats.clear();
            m_swapchain_support_info.present_modes.clear();
            DebugLog("Required swapchain support not present, skipping device {}.", properties.deviceName);
            return false;
        }

        u32 available_extension_count = 0;
        VulkanCheck(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extension_count, nullptr));
        VkExtensionProperties* available_extensions_properties = m_allocator->allocate_lot(VkExtensionProperties, available_extension_count);
        VulkanCheck(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &available_extension_count, available_extensions_properties));

        for (const auto& required_extension : requirements.extensions) {
            bool found_extension = false;

            for (u32 i = 0; i < available_extension_count; i++) {
                if (strcmp(available_extensions_properties[i].extensionName, required_extension) == 0) {
                    found_extension = true;
                    break;
                }
            }

            if (!found_extension) {
                DebugLog("Required extension not found: '{}', skipping device {}.", required_extension, properties.deviceName);
                m_allocator->free_lot(VkExtensionProperties, available_extensions_properties, available_extension_count);
                m_swapchain_support_info.formats.clear();
                m_swapchain_support_info.present_modes.clear();
                return false;
            }
        }
        m_allocator->free_lot(VkExtensionProperties, available_extensions_properties, available_extension_count);

        return true;
    }
}
