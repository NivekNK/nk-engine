#include "nkpch.h"

#include "nk/app.h"

namespace nk {
    App* App::s_instance = nullptr;

    void App::run() {
        DebugLog("App running...");
    }

    void App::exit() {

    }

    App::App(const ApplicationConfig& config) {
        Assert(s_instance == nullptr);
        s_instance = this;

        InfoLog("nk::App created > Name: {} | Pos: ({}, {}) | Size: ({}, {})",
                config.name, config.start_pos_x, config.start_pos_y, config.start_width, config.start_height);
    }
}
