#pragma once

#include "nk/entry_point.h"

int main(int argc, char** argv) {
    return nk::entry_point(argc, argv);
}

#define CREATE_APP(AppType, ...)                                             \
    nk::App* nk::app_create(nk::Allocator* allocator) {                      \
        auto app = allocator->construct(AppType __VA_OPT__(, ) __VA_ARGS__); \
        return app;                                                          \
    }                                                                        \
                                                                             \
    void nk::app_destroy(nk::Allocator* allocator, nk::App* application) {   \
        allocator->destroy(AppType, application);                            \
    }
