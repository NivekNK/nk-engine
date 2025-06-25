#pragma once

#include "vulkan/vk.h"
#include "collections/dyarr.h"

namespace nk {
    class Instance {
    public:
        Instance() = default;
        ~Instance() = default;

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;

        void init(cstr application_name, mem::Allocator* allocator, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        VkInstance get() { return m_instance; }
        VkInstance operator()() { return m_instance; }
        operator VkInstance() { return m_instance; }

    private:
        void create_instance(cstr application_name, mem::Allocator* allocator);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        void create_debug_messenger();
#endif

        VkAllocationCallbacks* m_vulkan_allocator;

        cl::dyarr<cstr> m_extensions;
        VkInstance m_instance;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
        VkDebugUtilsMessengerEXT m_debug_messenger;
#endif
    };
}
