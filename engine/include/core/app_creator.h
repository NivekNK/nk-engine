#pragma once

#include "core/entry_point.h"
#include "core/app.h"
#include "memory/allocator.h"

int main(int argc, char** argv) {
    return nk::entry_point(argc, argv);
}

#define CREATE_APP(AppType, ...)                                           \
    nk::App* nk::App::create(nk::mem::Allocator* allocator) {              \
        return allocator->construct_t(AppType __VA_OPT__(, ) __VA_ARGS__); \
    }                                                                      \
    void nk::App::destroy(nk::mem::Allocator* allocator, nk::App* app) {   \
        allocator->deconstruct_t(AppType, app);                            \
    }
