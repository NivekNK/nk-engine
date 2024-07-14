#pragma once

#include "renderer/vulkan/vulkan.h"
#include "core/dyarr.h"

namespace nk {
    class Instance {
    public:
        Instance() = default;

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        Instance(Instance&& other);
        Instance& operator=(Instance&& other);

        void init(cstr application_name, Allocator* allocator, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        VkInstance get() { return m_instance; }
        VkInstance operator()() { return m_instance; }
        operator VkInstance() { return m_instance; }

    private:
        void create_instance(cstr application_name);
#if defined(NK_DEBUG)
        void create_debug_messenger();
#endif

        Dyarr<cstr> m_extensions;

        VkAllocationCallbacks* m_allocator;
        VkInstance m_instance;
#if defined(NK_DEBUG)
        VkDebugUtilsMessengerEXT m_debug_messenger;
#endif
    };
}