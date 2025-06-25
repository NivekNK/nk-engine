#include "nkpch.h"

#include "vulkan/utils.h"

#include "vulkan/instance.h"

#if defined(NK_PLATFORM_WINDOWS)
    #include "platform/platform_win32.h"
    #include <vulkan/vulkan_win32.h>
#else
    #Error : Linux not yet implemented !
#endif

namespace nk::vk {
    void get_required_extensions(cl::dyarr<cstr>& extensions) {
#if defined(NK_PLATFORM_WINDOWS)
        extensions.dyarr_push_ptr("VK_KHR_win32_surface");
#else
    #Error : Linux not yet implemented !
#endif
    }

    VkSurfaceKHR create_surface(Platform* platform, Instance* instance, VkAllocationCallbacks* vulkan_allocator) {
#if defined(NK_PLATFORM_WINDOWS)
        PlatformWin32* native_platform = static_cast<PlatformWin32*>(platform);

        VkWin32SurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        create_info.hinstance = native_platform->get_hinstance();
        create_info.hwnd = native_platform->get_hwnd();

        VkSurfaceKHR surface;
        VkResult result = vkCreateWin32SurfaceKHR(instance->get(), &create_info, vulkan_allocator, &surface);
        if (result != VK_SUCCESS) {
            FatalLog("nk::vk::create_surface Vulkan surface creation failed.");
            return nullptr;
        }

        return surface;
#else
    #Error : Linux not yet implemented !
#endif
    }
}
