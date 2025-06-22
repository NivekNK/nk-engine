#include "nkpch.h"

#include "core/app.h"

#include "memory/malloc_allocator.h"
#include "platform/platform.h"
#include "systems/input_system.h"

namespace nk {
    App* App::s_instance = nullptr;

    App::~App() {
        Platform::free(m_allocator, m_platform);

        native_deconstruct(mem::MallocAllocator, m_allocator);
    }

    void App::run() {
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

    App::App(const ApplicationConfig& config) {
        Assert(s_instance == nullptr);
        s_instance = this;

        m_allocator = native_construct(mem::MallocAllocator);
        m_allocator->allocator_init(mem::MallocAllocator, "App", MemoryType::App);

        m_platform = Platform::create(m_allocator, config);
    }
}
