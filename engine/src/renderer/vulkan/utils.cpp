#include "nkpch.h"

#include "renderer/vulkan/utils.h"

namespace nk::Utils {
    void get_required_extensions(Dyarr<cstr>& extensions) {
#if defined(NK_PLATFORM_WINDOWS)
        extensions.push("VK_KHR_win32_surface");
#else
        #Error: Linux not yet implemented!
#endif
    }
}