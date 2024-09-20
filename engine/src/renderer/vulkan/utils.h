#pragma once

#include "vulkan/vk.h"
#include "core/dyarr.h"

namespace nk {
    class Instance;
    class Window;

    namespace vk {
        void get_required_extensions(Dyarr<cstr>& extensions);

        VkSurfaceKHR create_surface(Window& window, Instance* instance, VkAllocationCallbacks* vulkan_allocator);

        bool is_success(VkResult result);

        cstr result_to_cstr(VkResult result, bool get_extended);
    }
}