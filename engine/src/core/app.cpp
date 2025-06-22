#include "nkpch.h"

#include "core/app.h"

namespace nk {
    App::~App() {}

    App::App(ApplicationConfig config)
        : initial_config{config} {}
}
