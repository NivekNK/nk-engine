#include "nkpch.h"

#include "nk/window.h"

#include "nk/app.h"
#include "memory/allocator.h"

#if defined(NK_PLATFORM_WINDOWS)
    #include "nk/window_win32.h"
#else
    #error Linux not yet implemented!
#endif

namespace nk {
    Window* window_create(Allocator* allocator, const ApplicationConfig& config) {
#if defined(NK_PLATFORM_WINDOWS)
        return allocator->construct(WindowWin32, config);
#else
    #error Linux not yet implemented!
#endif
    }

    void window_destroy(Allocator* allocator, Window* window) {
#if defined(NK_PLATFORM_WINDOWS)
        allocator->destroy(WindowWin32, window);
#else
    #error Linux not yet implemented!
#endif
    }

    Window::Window(const ApplicationConfig& config)
        : m_suspended{false},
          m_running{true},
          m_pos_x{config.start_pos_x},
          m_pos_y{config.start_pos_y},
          m_width{config.start_width},
          m_height{config.start_height} {
    }
}
