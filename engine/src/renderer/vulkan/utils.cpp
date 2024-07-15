#include "nkpch.h"

#include "vulkan/utils.h"

#include "vulkan/instance.h"

#if defined(NK_PLATFORM_WINDOWS)
    #include "nk/window_win32.h"
    #include <vulkan/vulkan_win32.h>
#else
    #Error: Linux not yet implemented!
#endif

namespace nk::Utils {
    void get_required_extensions(Dyarr<cstr>& extensions) {
#if defined(NK_PLATFORM_WINDOWS)
        extensions.push("VK_KHR_win32_surface");
#else
        #Error: Linux not yet implemented!
#endif
    }

    VkSurfaceKHR create_surface(Window& window, Instance& instance, VkAllocationCallbacks* allocator) {
#if defined(NK_PLATFORM_WINDOWS)
        WindowWin32& native_window = static_cast<WindowWin32&>(window);

        VkWin32SurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        create_info.hinstance = native_window.get_hinstance();
        create_info.hwnd = native_window.get_hwnd();

        VkSurfaceKHR surface;
        VkResult result = vkCreateWin32SurfaceKHR(instance.get(), &create_info, allocator, &surface);
        if (result != VK_SUCCESS) {
            FatalLog("Vulkan surface creation failed.");
            return nullptr;
        }

        return surface;
#else
        #Error: Linux not yet implemented!
#endif
    }
}