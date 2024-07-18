#include "nkpch.h"

#include "nk/app.h"

#include "nk/window.h"
#include "memory/malloc_allocator.h"
#include "event/event.h"
#include "system/input_system.h"

#include "renderer/renderer.h"

namespace nk {
    App* App::s_instance = nullptr;

    App::~App() {
        Renderer::free(m_allocator, m_renderer);

        Window::free(m_allocator, m_window);

        delete m_allocator;
        InfoLog("nk::App destroyed.");
    }

    void App::run() {
        m_clock.start().update();

        m_last_time = m_clock.elapsed();
        f64 running_time = 0.0f;
        u8 frame_count = 0;
        f64 target_frame_seconds = 1.0f / 60;

        while (m_window->is_running()) {
            if (!m_window->pump_messages()) {
                // TODO: Close event
            }

            if (m_window->is_suspended())
                continue;

            m_clock.update();
            f64 current_time = m_clock.elapsed();
            f64 delta_time = current_time - m_last_time;
            f64 frame_start_time = m_window->get_absolute_time();

            if (!m_window->update(delta_time)) {
                FatalLog("nk::Window update failed. Shutting down!");
                break;
            }

            if (!m_window->render(delta_time)) {
                FatalLog("nk::Window render failed. Shutting down!");
                break;
            }

            // TODO: Refactor packet creation
            RenderPacket render_packet {
                .delta_time = delta_time,
            };
            m_renderer->draw_frame(render_packet);

            f64 frame_end_time = m_window->get_absolute_time();
            f64 frame_elapsed_time = frame_end_time - frame_start_time;
            running_time += frame_elapsed_time;

            f64 remaining_seconds = target_frame_seconds - frame_elapsed_time;
            if (remaining_seconds > 0) {
                u64 remaining_ms = static_cast<u64>(remaining_seconds * 1000);
                bool limit_frames = false;
                if (limit_frames && remaining_ms > 0) {
                    m_window->sleep(remaining_ms - 1);
                }
                frame_count++;
            }

            InputSystem::get().update(delta_time);

            m_last_time = current_time;
        }
    }

    void App::exit() {
        s_instance->m_window->close();
    }

    App::App(const ApplicationConfig& config) {
        Assert(s_instance == nullptr);
        s_instance = this;

        auto allocator = new MallocAllocator();
        allocator->allocator_init("App", MemoryType::App);
        m_allocator = allocator;

        m_window = Window::create(m_allocator, config);

        m_renderer = Renderer::create(m_allocator, *m_window, config.name);

        m_clock.init(m_window);

        Event::WindowClose::add_listener([]() {
            DebugLog("WindowCloseEvent");
        });

        Event::WindowResize::add_listener([](u16 width, u16 height) {
            DebugLog("WindowResizeEvent: ({}, {})", width, height);
        });

        Event::WindowFocus::add_listener([](bool focus) {
            str focus_value = focus ? "true" : "false";
            DebugLog("WindowFocusEvent: {}", focus_value);
        });

        // Event::WindowMoved::add_listener([](i16 x, i16 y) {
        //     DebugLog("WindowMovedEvent: ({}, {})", x, y);
        // });

        Event::KeyStateChanged::add_listener([](KeyState key_state) {
            str pressed_value = key_state.pressed ? "pressed" : "released";
            char value = static_cast<char>(key_state.keycode);
            DebugLog("KeyStateChangedEvent: {} {}", value, pressed_value);
        });

        Event::MouseButtonChanged::add_listener([](MouseButtonState state) {
            str button_value;
            switch (state.button) {
                case MouseButton::Left:
                    button_value = "Left Button";
                    break;
                case MouseButton::Right:
                    button_value = "Right Button";
                    break;
                case MouseButton::Middle:
                    button_value = "Middle Button";
                    break;
                default:
                    button_value = "Unknown Button";
                    break;
            }
            str pressed_value = state.pressed ? "pressed" : "released";
            DebugLog("MouseButtonChangedEvent: {} {}", button_value, pressed_value);
        });

        // Event::MousePositionChanged::add_listener([](u16 x, u16 y) {
        //     DebugLog("MousePositionChangedEvent: ({}, {})", x, y);
        // });

        InfoLog("nk::App created > Name: {} | Pos: ({}, {}) | Size: ({}, {})",
                config.name, config.start_pos_x, config.start_pos_y, config.start_width, config.start_height);
    }
}
