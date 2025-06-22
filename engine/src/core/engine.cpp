#include "nkpch.h"

#include "core/engine.h"

#include "memory/malloc_allocator.h"
#include "core/app.h"
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

    void Engine::init_impl() {
        m_allocator = native_construct(mem::MallocAllocator);
        m_allocator->allocator_init(mem::MallocAllocator, "App", MemoryType::App);

        m_app = App::create(m_allocator);
        m_platform = Platform::create(m_allocator, m_app->initial_config);

        EventSystem::get().register_event(SystemEventCode::ApplicationQuit, nullptr, on_event);
        EventSystem::get().register_event(SystemEventCode::KeyPressed, nullptr, on_key);
        EventSystem::get().register_event(SystemEventCode::KeyReleased, nullptr, on_key);
    }

    void Engine::shutdown_impl() {
        EventSystem::get().unregister_event(SystemEventCode::ApplicationQuit, nullptr, on_event);
        EventSystem::get().unregister_event(SystemEventCode::KeyPressed, nullptr, on_key);
        EventSystem::get().unregister_event(SystemEventCode::KeyReleased, nullptr, on_key);

        App::destroy(m_allocator, m_app);
        Platform::free(m_allocator, m_platform);
        native_deconstruct(mem::MallocAllocator, m_allocator);
    }

    void Engine::run_impl() {
        while (m_platform->running()) {
            if (!m_platform->pump_messages()) {
                m_platform->close();
            }

            if (!m_platform->suspended()) {
                if (!update(0)) {
                    FatalLog("nk::App::run update failed. shutting douwn.");
                    m_platform->close();
                    break;
                }

                if (!render(0)) {
                    FatalLog("nk::App::run render failed. shutting douwn.");
                    m_platform->close();
                    break;
                }

                InputSystem::get().update(0);
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
