#include "nkpch.h"

#include "core/engine.h"

#include "memory/malloc_allocator.h"
#include "core/app.h"
#include "platform/platform.h"
#include "renderer/renderer.h"
#include "systems/input_system.h"

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
            if (keycode == KeyCode::A) {
                DebugLog("Explicit - A key pressed!");
            } else {
                DebugLog("'{}' key pressed in window.", static_cast<char>(keycode));
            }
        } else if (code == SystemEventCode::KeyReleased) {
            // NOTE: Test code, remove later
            KeyCodeFlag keycode = context.data.u16[0];
            if (keycode == KeyCode::B) {
                DebugLog("Explicit - B key released!");
            } else {
                DebugLog("'{}' key released in window.", static_cast<char>(keycode));
            }
        }
        return false;
    }

    void Engine::exit_impl() {
        m_platform->close();
    }

    void Engine::init_impl() {
        m_allocator = native_construct(mem::MallocAllocator);
        m_allocator->allocator_init(mem::MallocAllocator, "App", MemoryType::App);

        m_app = App::create(m_allocator);
        m_platform = Platform::create(m_allocator, m_app->initial_config);
        m_renderer = Renderer::create(m_allocator, m_platform, m_app->initial_config.name);

        m_clock.init(m_platform);

        EventSystem::get().register_event(SystemEventCode::ApplicationQuit, nullptr, on_event);
        EventSystem::get().register_event(SystemEventCode::KeyPressed, nullptr, on_key);
        EventSystem::get().register_event(SystemEventCode::KeyReleased, nullptr, on_key);
    }

    void Engine::shutdown_impl() {
        EventSystem::get().unregister_event(SystemEventCode::ApplicationQuit, nullptr, on_event);
        EventSystem::get().unregister_event(SystemEventCode::KeyPressed, nullptr, on_key);
        EventSystem::get().unregister_event(SystemEventCode::KeyReleased, nullptr, on_key);

        Renderer::destroy(m_allocator, m_renderer);
        Platform::destroy(m_allocator, m_platform);
        App::destroy(m_allocator, m_app);
        native_deconstruct(mem::MallocAllocator, m_allocator);
    }

    void Engine::run_impl() {
        m_clock.start();
        m_clock.update();
        m_last_time = m_clock.elapsed();

        f64 running_time = 0;
        u8 frame_count = 0;
        f64 target_frame_seconds = 1.0f / 60;

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
                m_renderer->draw_frame({
                    .delta_time = delta,
                });

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

                    frame_count++;
                }

                InputSystem::update(delta);

                // Update last time
                m_last_time = current_time;
            }
        }

        m_platform->close();
    }

    bool Engine::update(f64 delta_time) {
        return m_app->update(delta_time);
    }

    bool Engine::render(f64 delta_time) {
        return m_app->render(delta_time);
    }
}
