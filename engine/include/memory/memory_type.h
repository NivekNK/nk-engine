#pragma once

#include "macros/map.h"

namespace nk {
    using MemoryTypeValue = u32;

    namespace MemoryType {
        enum {
            None,
            OriginalMaxMemoryTypes,
        };
    }

    cstr extended_memory_type_to_cstr(const MemoryTypeValue value);

#define _NK_EXPAND_AND_SET_MAX_MEMORY_TYPE(first_value, ...) \
    namespace MemoryType {                                   \
        enum {                                               \
            first_value = OriginalMaxMemoryTypes + 1,        \
            __VA_ARGS__ __VA_OPT__(, )                       \
                MaxMemoryTypes,                              \
        };                                                   \
    }

#define _NK_SWITCH_TO_STRING_MEMORY_TYPE(value) \
    case MemoryType::value: return #value;

#define NK_EXTEND_MEMORY_TYPE(...)                                          \
    _NK_EXPAND_AND_SET_MAX_MEMORY_TYPE(__VA_ARGS__)                         \
    inline cstr extended_memory_type_to_cstr(const MemoryTypeValue value) { \
        switch (value) {                                                    \
            NK_MAP(_NK_SWITCH_TO_STRING_MEMORY_TYPE, __VA_ARGS__)           \
        }                                                                   \
        return "Invalid";                                                   \
    }
}
