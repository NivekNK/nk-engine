#include "nkpch.h"

#include "core/engine.h"

#include "memory/malloc_allocator.h"
#include "systems/memory_system.h"
#include "core/app.h"
#include "platform/platform.h"
#include "renderer/renderer.h"
#include "systems/input_system.h"

// TODO: Temporal include
#include "core/camera.h"

namespace nk {
    bool on_event(SystemEventCode code, void* sender, void* listener, EventContext context) {
        switch (code) {
            case SystemEventCode::ApplicationQuit: {
                Engine::exit();
                return true;
            }
        }
        return false;
    }

    bool on_key(SystemEventCode code, void* sender, void* listener, EventContext context) {
        if (code == SystemEventCode::KeyPressed) {
            // NOTE: Test code, remove later
            KeyCodeFlag keycode = context.data.u16[0];
            switch (keycode) {
                case KeyCode::RAlt:
                    DebugLog("'Right Alt' key pressed in window.");
                    break;
                case KeyCode::LAlt:
                    DebugLog("'Left Alt' key pressed in window.");
                    break;
                case KeyCode::RShift:
                    DebugLog("'Right Shift' key pressed in window.");
                    break;
                case KeyCode::LShift:
                    DebugLog("'Left Shift' key pressed in window.");
                    break;
                case KeyCode::RCtrl:
                    DebugLog("'Right Ctrl' key pressed in window.");
                    break;
                case KeyCode::LCtrl:
                    DebugLog("'Left Ctrl' key pressed in window.");
                    break;
                case KeyCode::T: {
                    Engine& engine = Engine::get();
                    if (engine.m_renderer == nullptr)
                        break;
                    auto cycled = engine.m_renderer->cycle_debug_texture();
                    if (!cycled) {
                        const renderer_error& error = cycled.error();
                        ErrorLog(
                            "Texture cycle failed: renderer_error={}, native_code={}",
                            static_cast<u32>(error.code),
                            error.native_code);
                    }
                    return true;
                }
                default:
                    DebugLog("'{}' key pressed in window.", static_cast<char>(keycode));
                    break;
            }
        } else if (code == SystemEventCode::KeyReleased) {
            // NOTE: Test code, remove later
            KeyCodeFlag keycode = context.data.u16[0];
            switch (keycode) {
                case KeyCode::RAlt:
                    DebugLog("'Right Alt' key released in window.");
                    break;
                case KeyCode::LAlt:
                    DebugLog("'Left Alt' key released in window.");
                    break;
                case KeyCode::RShift:
                    DebugLog("'Right Shift' key released in window.");
                    break;
                case KeyCode::LShift:
                    DebugLog("'Left Shift' key released in window.");
                    break;
                case KeyCode::RCtrl:
                    DebugLog("'Right Ctrl' key released in window.");
                    break;
                case KeyCode::LCtrl:
                    DebugLog("'Left Ctrl' key released in window.");
                    break;
                case KeyCode::T:
                    return true;
                default:
                    DebugLog("'{}' key released in window.", static_cast<char>(keycode));
                    break;
            }
        }
        return false;
    }

    bool on_resized(SystemEventCode code, void* sender, void* listener, EventContext context) {
        if (code != SystemEventCode::Resized)
            return false;

        const u32 width = context.data.u32[0];
        const u32 height = context.data.u32[1];

        Engine& engine = Engine::get();
        return engine.resize(width, height);
    }

    void Engine::exit_impl() {
        if (m_platform != nullptr)
            m_platform->close();
    }

    bool Engine::init_impl() {
        if (m_initialized)
            return true;

        m_allocator = native_construct(mem::MallocAllocator);
        if (m_allocator == nullptr)
            return false;
        if (m_allocator->allocator_init(mem::MallocAllocator, "App", MemoryType::App) == nullptr) {
            native_deconstruct(mem::MallocAllocator, m_allocator);
            m_allocator = nullptr;
            return false;
        }

        m_app = App::create(m_allocator);
        if (m_app == nullptr) {
            shutdown_impl();
            return false;
        }

        m_platform = Platform::create(m_allocator, m_app->initial_config);
        if (m_platform == nullptr || !m_platform->running()) {
            shutdown_impl();
            return false;
        }

        auto renderer = Renderer::create(
            m_allocator,
            m_platform,
            m_app->initial_config.name);
        if (!renderer) {
            const renderer_error& error = renderer.error();
            ErrorLog(
                "Renderer initialization failed: renderer_error={}, native_code={}",
                static_cast<u32>(error.code),
                error.native_code);
            shutdown_impl();
            return false;
        }
        m_renderer = *renderer;

        Camera::init(m_renderer);

        m_clock.init(m_platform);

        EventSystem::register_event(SystemEventCode::ApplicationQuit, nullptr, on_event);
        EventSystem::register_event(SystemEventCode::KeyPressed, nullptr, on_key);
        EventSystem::register_event(SystemEventCode::KeyReleased, nullptr, on_key);
        EventSystem::register_event(SystemEventCode::Resized, nullptr, on_resized);

        m_initialized = true;
        return true;
    }

    void Engine::shutdown_impl() {
        if (m_initialized) {
            EventSystem::unregister_event(SystemEventCode::ApplicationQuit, nullptr, on_event);
            EventSystem::unregister_event(SystemEventCode::KeyPressed, nullptr, on_key);
            EventSystem::unregister_event(SystemEventCode::KeyReleased, nullptr, on_key);
            EventSystem::unregister_event(SystemEventCode::Resized, nullptr, on_resized);
        }

        if (m_renderer != nullptr) {
            Renderer::destroy(m_allocator, m_renderer);
            m_renderer = nullptr;
        }
        if (m_platform != nullptr) {
            Platform::destroy(m_allocator, m_platform);
            m_platform = nullptr;
        }
        if (m_app != nullptr) {
            App::destroy(m_allocator, m_app);
            m_app = nullptr;
        }
        if (m_allocator != nullptr) {
            native_deconstruct(mem::MallocAllocator, m_allocator);
            m_allocator = nullptr;
        }
        m_initialized = false;
    }

    void Engine::run_impl() {
        if (!m_initialized)
            return;

        m_clock.start();
        m_clock.update();
        m_last_time = m_clock.elapsed();

        f64 running_time = 0;
        u64 frame_count = 0;
        f64 target_frame_seconds = 1.0f / 60;
        u64 smoke_test_frames = 0;
#if NK_MEMORY_TRACKING_ENABLED
        u64 stable_frame_allocation_baseline = 0;
#endif
        if (const cstr configured_frames = std::getenv("NK_SMOKE_TEST_FRAMES");
            configured_frames != nullptr) {
            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(configured_frames, &end, 10);
            if (end != configured_frames && *end == '\0')
                smoke_test_frames = static_cast<u64>(parsed);
        }

        while (m_platform->running()) {
            if (!m_platform->pump_messages()) {
                m_platform->close();
            }

            if (!m_platform->suspended()) {
                // Update clock and get delta time
                m_clock.update();
                f64 current_time = m_clock.elapsed();
                f64 delta = current_time - m_last_time;
                f64 frame_start_time = m_platform->get_absolute_time();

                if (!update(delta)) {
                    FatalLog("nk::App::run update failed. shutting douwn.");
                    m_platform->close();
                    break;
                }

                if (!render(delta)) {
                    FatalLog("nk::App::run render failed. shutting douwn.");
                    m_platform->close();
                    break;
                }

                // TODO: refactor packet creation
                auto frame = m_renderer->draw_frame({
                    .delta_time = delta,
                });
                if (!frame) {
                    const renderer_error& error = frame.error();
                    ErrorLog(
                        "Rendering failed: renderer_error={}, native_code={}",
                        static_cast<u32>(error.code),
                        error.native_code);
                    m_platform->close();
                    break;
                }

                // Figure out how long the frame took
                f64 frame_end_time = m_platform->get_absolute_time();
                f64 frame_elapsed_time = frame_end_time - frame_start_time;
                running_time += frame_elapsed_time;
                f64 remaining_seconds = target_frame_seconds - frame_elapsed_time;

                if (remaining_seconds > 0) {
                    u64 remaining_ms = remaining_seconds * 1000;

                    // If there is time left, give it back to the OS
                    bool limit_frames = false;
                    if (remaining_ms > 0 && limit_frames) {
                        m_platform->sleep(remaining_ms - 1);
                    }

                }

                InputSystem::update(delta);

                ++frame_count;
#if NK_MEMORY_TRACKING_ENABLED
                if (smoke_test_frames > 1 && frame_count == 1) {
                    stable_frame_allocation_baseline =
                        mem::MemorySystem::get().allocation_event_count();
                }
#endif
                if (smoke_test_frames != 0 && frame_count >= smoke_test_frames)
                    m_platform->close();

                // Update last time
                m_last_time = current_time;
            }
        }

#if NK_MEMORY_TRACKING_ENABLED
        if (smoke_test_frames > 1) {
            const u64 allocation_events =
                mem::MemorySystem::get().allocation_event_count() -
                stable_frame_allocation_baseline;
            if (allocation_events == 0) {
                InfoLog(
                    "Stable-frame allocation check: 0 allocation events across {} checked frame(s).",
                    smoke_test_frames - 1);
            } else {
                ErrorLog(
                    "Stable-frame allocation check: {} unexpected allocation event(s) across {} checked frame(s).",
                    allocation_events,
                    smoke_test_frames - 1);
            }
        }
#endif

        m_platform->close();
    }

    bool Engine::update(f64 delta_time) {
        return m_app->update(delta_time);
    }

    bool Engine::render(f64 delta_time) {
        return m_app->render(delta_time);
    }

    bool Engine::resize(u32 width, u32 height) {
        if (width != m_platform->width() || height != m_platform->height()) {
            m_platform->on_resized(width, height);
            if (width == 0 || height == 0) {
                InfoLog("nk::Engine::on_resized Window minimized, suspending application.");
                m_platform->set_suspended(true);
            } else {
                if (m_platform->suspended()) {
                    InfoLog("nk::Engine::on_resized Window restored, resuming application.");
                    m_platform->set_suspended(false);
                }
                m_app->on_resized(width, height);
                m_renderer->resize(width, height);
            }
            return true;
        }
        return false;
    }
}
