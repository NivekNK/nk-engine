#include "nkpch.h"

#include "platform/platform.h"

#include "core/app.h"
#include "memory/allocator.h"

#if defined(NK_PLATFORM_WINDOWS)
    #include "platform/platform_win32.h"
#elif defined(NK_PLATFORM_LINUX)
    #include "platform/platform_linux.h"
    #include "platform/platform_wayland.h"
#else
    #error Platform not supported!
#endif

namespace nk {
    Platform* Platform::create(mem::Allocator* allocator, const ApplicationConfig& config) {
        if (allocator == nullptr)
            return nullptr;
#if defined(NK_PLATFORM_WINDOWS)
        return allocator->construct_t(PlatformWin32, config);
#elif defined(NK_PLATFORM_LINUX)
        const cstr requested_backend = std::getenv("NK_PLATFORM_BACKEND");
        const cstr wayland_display = std::getenv("WAYLAND_DISPLAY");
        const bool force_xcb = requested_backend != nullptr && std::strcmp(requested_backend, "xcb") == 0;
        const bool prefer_wayland = !force_xcb &&
                                    ((requested_backend != nullptr && std::strcmp(requested_backend, "wayland") == 0) ||
                                     (wayland_display != nullptr && std::strlen(wayland_display) > 0));

        if (requested_backend != nullptr && !force_xcb && std::strcmp(requested_backend, "wayland") != 0) {
            WarnLog("Unknown NK_PLATFORM_BACKEND value '{}'; selecting the backend automatically.", requested_backend);
        }

        if (prefer_wayland) {
            PlatformWayland* wayland = allocator->construct_t(PlatformWayland, config);
            if (wayland != nullptr && wayland->running())
                return wayland;

            WarnLog("Native Wayland initialization failed; falling back to XCB/XWayland.");
            if (wayland != nullptr)
                allocator->deconstruct_t(PlatformWayland, wayland);
        }

        return allocator->construct_t(PlatformLinux, config);
#else
    #error Platform not supported!
#endif
    }
    
    void Platform::destroy(mem::Allocator* allocator, Platform* platform) {
        if (allocator == nullptr || platform == nullptr)
            return;
#if defined(NK_PLATFORM_WINDOWS)
        allocator->deconstruct_t(PlatformWin32, platform);
#elif defined(NK_PLATFORM_LINUX)
        if (platform->backend() == PlatformBackend::Wayland)
            allocator->deconstruct_t(PlatformWayland, platform);
        else
            allocator->deconstruct_t(PlatformLinux, platform);
#else
    #error Platform not supported!
#endif
    }

    Platform::Platform(const ApplicationConfig& config)
        : m_suspended{false},
          m_running{true},
          m_pos_x{config.start_pos_x},
          m_pos_y{config.start_pos_y},
          m_width{config.start_width},
          m_height{config.start_height} {}
}
