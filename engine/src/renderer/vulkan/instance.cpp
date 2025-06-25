#include "nkpch.h"

#include "vulkan/instance.h"

#include "vulkan/utils.h"

namespace nk {
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
    VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_types,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data) {
        switch (message_severity) {
            default:
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                ErrorLog(callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                WarnLog(callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                InfoLog(callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                TraceLog(callback_data->pMessage);
                break;
        }
        return VK_FALSE;
    }
#endif

    void Instance::init(cstr application_name, mem::Allocator* allocator, VkAllocationCallbacks* vulkan_allocator) {
        m_vulkan_allocator = vulkan_allocator;
        m_extensions.dyarr_init(allocator, 12);
        create_instance(application_name, allocator);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        create_debug_messenger();
#endif
        TraceLog("nk::Instance initialized.");
    }

    void Instance::shutdown() {
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        if (m_debug_messenger) {
            PFN_vkDestroyDebugUtilsMessengerEXT func =
                (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
            func(m_instance, m_debug_messenger, m_vulkan_allocator);
        }
        DebugLog("Vulkan Debug Messenger destroyed.");
#endif

        vkDestroyInstance(m_instance, m_vulkan_allocator);
        InfoLog("Vulkan Instance destroyed.");

        m_extensions.dyarr_shutdown();
        InfoLog("Vulkan extensions freed.");

        m_vulkan_allocator = nullptr;
        TraceLog("nk::Instance shutdown.");
    }

    void Instance::create_instance(cstr application_name, mem::Allocator* allocator) {
        VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app_info.apiVersion = VK_API_VERSION_1_2;
        app_info.pApplicationName = application_name;
        app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        app_info.pEngineName = "nk-engine";
        app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);

        VkInstanceCreateInfo instance_create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance_create_info.pApplicationInfo = &app_info;

        m_extensions.dyarr_push_ptr(VK_KHR_SURFACE_EXTENSION_NAME);
        vk::get_required_extensions(m_extensions);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        m_extensions.dyarr_push_ptr(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        str debug_extensions = "Required extensions:\n";
        for (u64 i = 0; i < m_extensions.length(); i++) {
            debug_extensions += "                        ";
            debug_extensions += m_extensions[i];
            debug_extensions += "\n";
        }
        DebugLog(debug_extensions);
#endif

        instance_create_info.enabledExtensionCount = static_cast<u32>(m_extensions.length());
        instance_create_info.ppEnabledExtensionNames = m_extensions.data();

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        DebugLog("Validation layers enabled. Enumerating...");

        // The list of validation layers required.
        const u8 required_validation_layers_count = 1;
        const cstr required_validation_layers[required_validation_layers_count] = {
            "VK_LAYER_KHRONOS_validation",
        };

        // Obtain a list of available validation layers
        u32 available_layer_count = 0;
        VulkanCheck(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
        VkLayerProperties* available_layers = allocator->allocate_lot_t(VkLayerProperties, available_layer_count);
        VulkanCheck(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

        for (cstr layer_name : required_validation_layers) {
            bool layer_found = false;

            for (u32 i = 0; i < available_layer_count; i++) {
                if (strcmp(layer_name, available_layers[i].layerName) == 0) {
                    layer_found = true;
                    DebugLog("Layer {} found.", layer_name);
                    break;
                }
            }

            if (!layer_found) {
                FatalLog("Required validation layer is missing: {}.", layer_name);
                return;
            }
        }

        allocator->free_lot_t(VkLayerProperties, available_layers, available_layer_count);

        DebugLog("All required validation layers are present.");

        instance_create_info.enabledLayerCount = required_validation_layers_count;
        instance_create_info.ppEnabledLayerNames = required_validation_layers;
#else
        instance_create_info.enabledLayerCount = 0;
        instance_create_info.ppEnabledLayerNames = nullptr;
#endif

        VulkanCheck(vkCreateInstance(&instance_create_info, m_vulkan_allocator, &m_instance));
        InfoLog("Vulkan Instance created.");
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
    void Instance::create_debug_messenger() {
        u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug_create_info.messageSeverity = log_severity;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debug_create_info.pfnUserCallback = vk_debug_callback;

        PFN_vkCreateDebugUtilsMessengerEXT func =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        Assert(func != nullptr, "Failed to create debug messenger!");
        VulkanCheck(func(m_instance, &debug_create_info, m_vulkan_allocator, &m_debug_messenger));
        DebugLog("Vulkan debugger created.");
    }
#endif
}
