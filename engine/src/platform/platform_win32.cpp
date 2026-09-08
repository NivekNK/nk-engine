#include "nkpch.h"

#include "platform/platform_win32.h"

#include "core/app.h"
#include "systems/event_system.h"
#include "systems/input_system.h"
#include "core/utf8.h"
#include <imm.h>
#include <algorithm>
#include <cmath>

namespace {
    void submit_utf16(const wchar_t* input, const int units, const bool preedit,
        const int selection_units = 0) {
        if (input == nullptr || units < 0) return;
        char utf8[2048]{};
        const int bytes = units == 0 ? 0 : WideCharToMultiByte(CP_UTF8,
            WC_ERR_INVALID_CHARS, input, units, utf8, sizeof(utf8), nullptr, nullptr);
        if (units != 0 && bytes <= 0) return;
        if (!preedit) {
            (void)nk::InputSystem::process_text({utf8, static_cast<nk::u64>(bytes)});
            return;
        }
        const int bounded_selection = std::clamp(selection_units, 0, units);
        const int selected_bytes = bounded_selection == 0 ? 0 : WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, input, bounded_selection,
            nullptr, 0, nullptr, nullptr);
        if (selected_bytes < 0) return;
        (void)nk::InputSystem::process_preedit({utf8, static_cast<nk::u64>(bytes)},
            static_cast<nk::u32>(selected_bytes), static_cast<nk::u32>(selected_bytes));
    }

    void submit_ime(HWND window, const LPARAM flags) {
        HIMC context = ImmGetContext(window);
        if (context == nullptr) return;
        wchar_t text[512]{};
        if ((flags & GCS_RESULTSTR) != 0) {
            const LONG bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR,
                text, sizeof(text));
            if (bytes >= 0 && bytes <= static_cast<LONG>(sizeof(text)))
                submit_utf16(text, bytes / sizeof(wchar_t), false);
            (void)nk::InputSystem::process_preedit({}, 0, 0);
        }
        if ((flags & GCS_COMPSTR) != 0) {
            const LONG bytes = ImmGetCompositionStringW(context, GCS_COMPSTR,
                text, sizeof(text));
            const LONG cursor = ImmGetCompositionStringW(context, GCS_CURSORPOS,
                nullptr, 0);
            if (bytes >= 0 && bytes <= static_cast<LONG>(sizeof(text)))
                submit_utf16(text, bytes / sizeof(wchar_t), true,
                    cursor < 0 ? bytes / sizeof(wchar_t) : cursor);
        }
        ImmReleaseContext(window, context);
    }
}

namespace nk {
    LRESULT CALLBACK win32_process_message(HWND hwnd, u32 msg, WPARAM wparam, LPARAM lparam);

    PlatformWin32::PlatformWin32(const ApplicationConfig& config)
        : Platform(config) {
        // Windows 11 per-monitor awareness keeps framebuffer and logical UI
        // coordinates explicit when a window crosses monitors.
        (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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
        const UINT initial_dpi = GetDpiForSystem();
        set_content_scale(static_cast<f32>(initial_dpi) / 96.f);
        RECT border_rect = {
            0,
            0,
            static_cast<LONG>(std::ceil(config.start_width * content_scale())),
            static_cast<LONG>(std::ceil(config.start_height * content_scale())),
        };
        AdjustWindowRectExForDpi(&border_rect, window_style, FALSE,
            window_ex_style, initial_dpi);

        window_x += border_rect.left;
        window_y += border_rect.top;
        const u32 window_width = static_cast<u32>(border_rect.right - border_rect.left);
        const u32 window_height = static_cast<u32>(border_rect.bottom - border_rect.top);

        HWND handle = CreateWindowExA(
            window_ex_style,
            "nk_window_class",
            config.name.c_str(),
            window_style,
            window_x,
            window_y,
            window_width,
            window_height,
            0, 0, m_hinstance, this
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
        set_content_scale(static_cast<f32>(GetDpiForWindow(handle)) / 96.f);

        bool should_activate = true; // TODO: if the window should not accept input, this should be false.
        i32 show_window_command_flags = should_activate ? SW_SHOW : SW_SHOWNOACTIVATE;
        // TODO:
        // If initally minimized, use WS_MINIMIZE : WS_SHOWMINOACTIVATE
        // If initilly maxsimized, use SW_SHOWMAXIMIZED : WS_MAXIMIZE
        ShowWindow(m_hwnd, show_window_command_flags);
        RECT client{};
        if (GetClientRect(m_hwnd, &client)) {
            m_width = static_cast<u32>(client.right - client.left);
            m_height = static_cast<u32>(client.bottom - client.top);
        }

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
        PlatformWin32* platform = reinterpret_cast<PlatformWin32*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            platform = static_cast<PlatformWin32*>(
                reinterpret_cast<CREATESTRUCTA*>(lparam)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(platform));
        }
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
            case WM_DPICHANGED: {
                if (platform != nullptr) {
                    platform->set_content_scale(
                        static_cast<f32>(LOWORD(wparam)) / 96.f);
                    const RECT& suggested = *reinterpret_cast<const RECT*>(lparam);
                    SetWindowPos(hwnd, nullptr, suggested.left, suggested.top,
                        suggested.right - suggested.left,
                        suggested.bottom - suggested.top,
                        SWP_NOACTIVATE | SWP_NOZORDER);
                }
                return 0;
            }
            case WM_ACTIVATEAPP: {
                if (!wparam) {
                    InputSystem::clear_keyboard();
                    (void)InputSystem::process_preedit({}, 0, 0);
                }
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

                // Check for extended scan code.
                bool is_extended = (HIWORD(lparam) & KF_EXTENDED) == KF_EXTENDED;

                // Keypres only determines if any alt/ctrl/shift/super key is pressed.
                if (wparam == VK_MENU) {
                    keycode = is_extended ? KeyCode::RAlt : KeyCode::LAlt;
                } else if (wparam == VK_CONTROL) {
                    keycode = is_extended ? KeyCode::RCtrl : KeyCode::LCtrl;
                } else if (wparam == VK_SHIFT) {
                    // KF_EXTENDED is not set for shift keys.
                    u32 left_shift = MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
                    u32 scan_code = (lparam & (0xFF << 16)) >> 16;
                    keycode = scan_code == left_shift ? KeyCode::LShift : KeyCode::RShift;
                }

                if (pressed && (lparam & (1ll << 30)) != 0)
                    InputSystem::process_key_repeat(keycode);
                else
                    InputSystem::process_key(keycode, pressed);
            } break;
            case WM_UNICHAR: {
                if (wparam == UNICODE_NOCHAR) return TRUE;
                char encoded[4]{};
                const u8 count = utf8::encode(static_cast<u32>(wparam), encoded);
                if (count) (void)InputSystem::process_text({encoded, count});
                return 0;
            }
            case WM_CHAR: {
                if (platform == nullptr) return 0;
                const wchar_t value = static_cast<wchar_t>(wparam);
                if (value >= 0xd800 && value <= 0xdbff) {
                    platform->m_pending_high_surrogate = value;
                    return 0;
                }
                u32 scalar = value;
                if (value >= 0xdc00 && value <= 0xdfff) {
                    if (platform->m_pending_high_surrogate == 0) return 0;
                    scalar = 0x10000 +
                        ((platform->m_pending_high_surrogate - 0xd800) << 10) +
                        (value - 0xdc00);
                } else if (platform->m_pending_high_surrogate != 0) {
                    platform->m_pending_high_surrogate = 0;
                }
                platform->m_pending_high_surrogate = 0;
                if (scalar >= 0x20 && scalar != 0x7f) {
                    char encoded[4]{};
                    const u8 count = utf8::encode(scalar, encoded);
                    if (count) (void)InputSystem::process_text({encoded, count});
                }
                return 0;
            }
            case WM_IME_COMPOSITION:
                submit_ime(hwnd, lparam);
                return 0;
            case WM_IME_ENDCOMPOSITION:
                (void)InputSystem::process_preedit({}, 0, 0);
                break;
            case WM_MOUSEMOVE: {
                const f32 scale = platform != nullptr ? platform->content_scale() : 1.f;
                i16 x_position = static_cast<i16>(GET_X_LPARAM(lparam) / scale);
                i16 y_position = static_cast<i16>(GET_Y_LPARAM(lparam) / scale);
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
