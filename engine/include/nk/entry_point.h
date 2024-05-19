#pragma once

#include "nk/app.h"
#include "memory/allocator.h"

namespace nk {
    App* app_create(Allocator* allocator);
    void app_destroy(Allocator* allocator, App* application);

    i32 entry_point(i32 argc, char** argv);
}
