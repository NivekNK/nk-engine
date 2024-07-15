#pragma once

#include "vulkan/vk.h"
#include "core/dyarr.h"

namespace nk {
    class Instance;
    class Window;

    namespace Utils {
        void get_required_extensions(Dyarr<cstr>& extensions);

        VkSurfaceKHR create_surface(Window& window, Instance& instance, VkAllocationCallbacks* allocator);
    }
}