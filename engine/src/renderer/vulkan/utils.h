#pragma once

#include "vulkan/vk.h"
#include "collections/dyarr.h"

namespace nk {
    class Platform;
    class Instance;

    namespace vk {
        void get_required_extensions(cl::dyarr<cstr>& extensions);

        VkSurfaceKHR create_surface(Platform* platform, Instance* instance, VkAllocationCallbacks* vulkan_allocator);
   
        cstr result_to_cstr(VkResult result, bool get_extended);
    }
}
