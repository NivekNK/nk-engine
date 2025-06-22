#include "nkpch.h"

#include "core/application.h"
#include "platform/platform.h"
#include "core/input_codes.h"

namespace nk {
    void Application::create(mem::Allocator* allocator) {
        App::create(allocator);
        EventSystem::get().register_event(SystemEventCode::ApplicationQuit, nullptr, Application::on_event);
        EventSystem::get().register_event(SystemEventCode::KeyPressed, nullptr, Application::on_key);
        EventSystem::get().register_event(SystemEventCode::KeyReleased, nullptr, Application::on_key);
    }

    void Application::destroy(mem::Allocator* allocator) {
        EventSystem::get().unregister_event(SystemEventCode::ApplicationQuit, nullptr, Application::on_event);
        EventSystem::get().unregister_event(SystemEventCode::KeyPressed, nullptr, Application::on_key);
        EventSystem::get().unregister_event(SystemEventCode::KeyReleased, nullptr, Application::on_key);
        App::destroy(allocator);
    }

    bool Application::on_event(SystemEventCode code, void* sender, void* listener, EventContext context) {
        switch (code) {
            case SystemEventCode::ApplicationQuit: {
                App::s_instance->m_platform->close();
                return true;
            }
        }
        return false;
    }

    bool Application::on_key(SystemEventCode code, void* sender, void* listener, EventContext context) {
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
}
