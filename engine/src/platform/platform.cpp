#include "nkpch.h"

#include "platform/platform.h"

#include "core/app.h"
#include "memory/allocator.h"

#if defined(NK_PLATFORM_WINDOWS)
    #include "platform/platform_win32.h"
#else
    #error Linux not yet implemented!
#endif

namespace nk {
    Platform* Platform::create(mem::Allocator* allocator, const ApplicationConfig& config) {
#if defined(NK_PLATFORM_WINDOWS)
        return allocator->construct_t(PlatformWin32, config);
#else
    #error Linux not yet implemented!
#endif
    }
    
    void Platform::free(mem::Allocator* allocator, Platform* platform) {
#if defined(NK_PLATFORM_WINDOWS)
        allocator->deconstruct_t(PlatformWin32, platform);
#else
    #error Linux not yet implemented!
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
