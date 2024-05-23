#include "nkpch.h"

#include "nk/app.h"

#include "nk/window.h"
#include "memory/malloc_allocator.h"

namespace nk {
    App* App::s_instance = nullptr;

    App::~App() {
        window_destroy(m_allocator, m_window);

        delete m_allocator;
    }

    void App::run() {
        DebugLog("App running...");
    }

    void App::exit() {

    }

    App::App(const ApplicationConfig& config) {
        Assert(s_instance == nullptr);
        s_instance = this;

        auto allocator = new MallocAllocator();
        allocator->allocator_init("App", MemoryType::App);
        m_allocator = allocator;

        m_window = window_create(m_allocator, config);

        InfoLog("nk::App created > Name: {} | Pos: ({}, {}) | Size: ({}, {})",
                config.name, config.start_pos_x, config.start_pos_y, config.start_width, config.start_height);
    }
}
