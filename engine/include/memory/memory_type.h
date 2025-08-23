#pragma once

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #include "macros/map.h"

    #define _NK_SWITCH_TO_STRING_MEMORY_TYPE(value) \
        case MemoryType::value: return #value;

    #define _NK_DEFINE_MEMORY_TYPE(...)                                                   \
        namespace nk::MemoryType {                                                        \
            using Value = u32;                                                            \
            enum : Value {                                                                \
                None,                                                                     \
                __VA_ARGS__ __VA_OPT__(, )                                                \
                    OriginalMaxMemoryTypes,                                               \
            };                                                                            \
            namespace Internal {                                                          \
                static std::function<cstr(Value)> extended_to_cstr;                       \
                static std::function<Value()> max;                                        \
            }                                                                             \
            inline Value max() {                                                          \
                if (Internal::max)                                                        \
                    return Internal::max();                                               \
                return OriginalMaxMemoryTypes;                                            \
            }                                                                             \
            inline nk::cstr to_cstr(const Value value) {                                  \
                if (Internal::extended_to_cstr && value > OriginalMaxMemoryTypes)         \
                    return Internal::extended_to_cstr(value);                             \
                switch (value) {                                                          \
                    case MemoryType::None:                                                \
                        return "None";                                                    \
                        __VA_OPT__(NK_MAP(_NK_SWITCH_TO_STRING_MEMORY_TYPE, __VA_ARGS__)) \
                }                                                                         \
                return "Invalid";                                                         \
            }                                                                             \
        }

    #define _NK_GET_REST(first, ...) __VA_ARGS__ __VA_OPT__(, )

    #define NK_EXTEND_MEMORY_TYPE(...)                                                           \
        __VA_OPT__(                                                                              \
            namespace nk::MemoryType {                                                           \
                enum {                                                                           \
                    NK_1ST_ARGUMENT(__VA_ARGS__) = OriginalMaxMemoryTypes + 1,                   \
                    _NK_GET_REST(__VA_ARGS__)                                                    \
                        MaxMemoryTypes,                                                          \
                };                                                                               \
                inline constexpr nk::MemoryType::Value extended_max() { return MaxMemoryTypes; } \
                inline nk::cstr extended_to_cstr(nk::MemoryType::Value value) {                  \
                    switch (value) {                                                             \
                        NK_MAP(_NK_SWITCH_TO_STRING_MEMORY_TYPE, __VA_ARGS__)                    \
                    }                                                                            \
                    return "Invalid";                                                            \
                }                                                                                \
            })

_NK_DEFINE_MEMORY_TYPE(Native, Test, System, Event, App, Renderer)

namespace nk {
    void memory_system_extended_memory_type(const std::function<nk::MemoryType::Value()>& max_memory_type, const std::function<cstr(MemoryType::Value)>& memory_type_to_cstr);
}

    #define NK_MEMORY_SYSTEM_EXTENDED_MEMORY_TYPE() \
        nk::memory_system_extended_memory_type(nk::MemoryType::extended_max, nk::MemoryType::extended_to_cstr)

#else

    #define NK_EXTEND_MEMORY_TYPE(...)

    #define NK_MEMORY_SYSTEM_EXPANDED_MEMORY_TYPE()

#endif
