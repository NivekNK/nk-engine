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

            // TODO: RenderPacket with Renderer Draw Frame

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

            // TODO: Update Input

            m_last_time = current_time;
        }

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

        m_clock.init(m_window);

        InfoLog("nk::App created > Name: {} | Pos: ({}, {}) | Size: ({}, {})",
                config.name, config.start_pos_x, config.start_pos_y, config.start_width, config.start_height);
    }
}
