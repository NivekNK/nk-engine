#include "nkpch.h"

#include "nk/window_win32.h"

#include "nk/app.h"

namespace nk {
    LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM wparam, LPARAM lparam);

    WindowWin32::WindowWin32(const ApplicationConfig& config)
        : Window(config) {
        m_hinstance = GetModuleHandleA(0);

        // Setup and register window class
        HICON icon = LoadIcon(m_hinstance, IDI_APPLICATION);
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_DBLCLKS; // Get double clicks
        wc.lpfnWndProc = win32_process_message;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = m_hinstance;
        wc.hIcon = icon;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW); // Manage the cursor manually
        wc.hbrBackground = NULL; // Transparent
        wc.lpszClassName = "nk_window_class";

        if (!RegisterClassA(&wc)) {
            MessageBoxA(0, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
            AssertMsg(false, "Window registration failed");
            return;
        }

        u32 window_style = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION |
                           WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME;

        u32 window_ex_style = WS_EX_APPWINDOW;

        i16 window_x = config.start_pos_x;
        i16 window_y = config.start_pos_y;
        u16 window_width = config.start_width;
        u16 window_height = config.start_height;

        RECT border_rect = {0, 0, 0, 0};
        AdjustWindowRectEx(&border_rect, window_style, 0, window_ex_style);

        window_x += border_rect.left;
        window_y += border_rect.right;

        // Grow by the size of the OS border.
        window_width += border_rect.right - border_rect.left;
        window_height += border_rect.bottom - border_rect.top;

        HWND handle = CreateWindowExA(
            window_ex_style,
            "nk_window_class",
            config.name.c_str(),
            window_style,
            window_x,
            window_y,
            window_width,
            window_height,
            0, 0, m_hinstance, 0
        );

        if (handle == 0) {
            MessageBoxA(NULL, "Window creation failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
            AssertMsg(false, "Window creation failed!");
            return;
        }
        m_hwnd = handle;

        bool should_activate = true; // TODO: if the window should not accept input, this should be false.
        i32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
        // TODO:
        // If initally minimized, use WS_MINIMIZE : WS_SHOWMINOACTIVATE
        // If initilly maxsimized, use SW_SHOWMAXIMIZED : WS_MAXIMIZE
        ShowWindow(m_hwnd, show_window_command_flags);

        // Clock setup
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        m_clock_frequency = 1.0f / static_cast<f64>(frequency.QuadPart);
        QueryPerformanceCounter(&m_start_time);
    }

    WindowWin32::~WindowWin32() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
        }
    }

    bool WindowWin32::pump_messages() {
        MSG message;
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        return true;
    }

    f64 WindowWin32::get_absolute_time() {
        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        return static_cast<f64>(current_time.QuadPart) * m_clock_frequency;
    }

    void WindowWin32::sleep(u64 ms) {
        Sleep(ms);
    }

    LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM wparam, LPARAM lparam) {
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}
