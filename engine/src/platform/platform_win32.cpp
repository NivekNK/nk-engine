#include "nkpch.h"

#include "platform/platform_win32.h"

#include "core/app.h"
#include "systems/event_system.h"
#include "systems/input_system.h"

namespace nk {
    LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM wparam, LPARAM lparam);

    PlatformWin32::PlatformWin32(const ApplicationConfig& config)
        : Platform(config) {
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
        wc.hbrBackground = NULL;                  // Transparent
        wc.lpszClassName = "nk_window_class";

        if (!RegisterClassA(&wc)) {
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
            Assert(false, "Window registration failed");
#else
            MessageBoxA(0, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
#endif
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

        if (handle == nullptr) {
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO
            Assert(false, "Window creation failed!");
#else
            MessageBoxA(NULL, "Window creation failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
#endif
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

        InfoLog("PlatformWin32 created.");
    }

    PlatformWin32::~PlatformWin32() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
        }
        InfoLog("PlatformWin32 destroyed.");
    }

    bool PlatformWin32::pump_messages() {
        MSG message;
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        return true;
    }

    f64 PlatformWin32::get_absolute_time() {
        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        return static_cast<f64>(current_time.QuadPart) * m_clock_frequency;
    }

    void PlatformWin32::sleep(u64 ms) {
        Sleep(ms);
    }

    LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM wparam, LPARAM lparam) {
        switch (msg) {
            case WM_ERASEBKGND:
                // Notify the OS that erasing will be handled by the application to prevent flicker.
                return 1;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_CLOSE:
                EventSystem::fire_event(SystemEventCode::ApplicationQuit, nullptr, EventContext{});
                return 0;
            case WM_SIZE: {
                WINDOWPLACEMENT placement;
                placement.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(hwnd, &placement);

                u32 width = 0;
                u32 height = 0;

                if (placement.showCmd != SW_SHOWMINIMIZED) {
                    width = LOWORD(lparam);
                    height = HIWORD(lparam);
                }

                EventContext context;
                context.data.u32[0] = width;
                context.data.u32[1] = height;
                EventSystem::fire_event(SystemEventCode::Resized, nullptr, context);
            } break;
            case WM_ACTIVATEAPP: {
                // if (wparam) {
                //     EventSystem::get().window_focus.invoke(true);
                // } else {
                //     EventSystem::get().window_focus.invoke(false);
                // }
            } break;
            case WM_WINDOWPOSCHANGING: {
                // RECT rect;
                // GetWindowRect(hwnd, &rect);
                // i16 x = rect.left;
                // i16 y = rect.top;
                // EventSystem::get().window_moved.invoke(x, y);
            } break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                // Key pressed or released
                bool pressed = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
                KeyCodeFlag keycode = static_cast<KeyCodeFlag>(wparam);

                if (wparam == VK_MENU) {
                    //bool is_right_alt = lparam & (1 << 24);  // Check the 24th bit of lParam
                    if (GetKeyState(VK_RMENU) & 0x8000) {
                        keycode = KeyCode::RAlt;
                    } else if (GetKeyState(VK_LMENU) & 0x8000) {
                        keycode = KeyCode::LAlt;
                    }
                } else if (wparam == VK_SHIFT) {
                    if (GetKeyState(VK_RSHIFT) & 0x8000) {
                        keycode = KeyCode::RShift;
                    } else if (GetKeyState(VK_LSHIFT) & 0x8000) {
                        keycode = KeyCode::LShift;
                    }
                } else if (wparam == VK_CONTROL) {
                    if (GetKeyState(VK_RCONTROL) & 0x8000) {
                        keycode = KeyCode::RCtrl;
                    } else if (GetKeyState(VK_LCONTROL) & 0x8000) {
                        keycode = KeyCode::LCtrl;
                    }
                }

                InputSystem::process_key(keycode, pressed);
            } break;
            case WM_MOUSEMOVE: {
                i16 x_position = GET_X_LPARAM(lparam);
                i16 y_position = GET_Y_LPARAM(lparam);
                InputSystem::process_mouse_move(x_position, y_position);
            } break;
            case WM_MOUSEWHEEL: {
                f32 z_delta = GET_WHEEL_DELTA_WPARAM(wparam);
                if (z_delta != 0) {
                    // Flatten the input to an OS-independent (-1, 1)
                    // z_delta /= 120.0f;
                    i8 z_delta_normalized = z_delta < 0 ? -1 : 1;
                    InputSystem::process_mouse_wheel(z_delta_normalized);
                }
            } break;
            case WM_LBUTTONDOWN: {
                InputSystem::process_mouse_button(MouseButton::Left, true);
            } break;
            case WM_MBUTTONDOWN: {
                InputSystem::process_mouse_button(MouseButton::Middle, true);
            } break;
            case WM_RBUTTONDOWN: {
                InputSystem::process_mouse_button(MouseButton::Right, true);
            } break;
            case WM_LBUTTONUP: {
                InputSystem::process_mouse_button(MouseButton::Left, false);
            } break;
            case WM_MBUTTONUP: {
                InputSystem::process_mouse_button(MouseButton::Middle, false);
            } break;
            case WM_RBUTTONUP: {
                InputSystem::process_mouse_button(MouseButton::Right, false);
            } break;
        }

        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}
