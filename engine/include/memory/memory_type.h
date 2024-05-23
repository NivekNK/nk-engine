#pragma once

#if !defined(NK_RELEASE)

#include "macros/map.h"

#define _NK_SWITCH_TO_STRING_MEMORY_TYPE(value) \
    case MemoryType::value: return #value;

#define _NK_DEFINE_MEMORY_TYPE(...)                                                       \
    namespace nk {                                                                        \
        using MemoryTypeValue = u32;                                                      \
        namespace MemoryType {                                                            \
            enum {                                                                        \
                None,                                                                     \
                __VA_ARGS__ __VA_OPT__(, )                                                \
                    OriginalMaxMemoryTypes,                                               \
            };                                                                            \
            inline constexpr nk::u32 max() { return OriginalMaxMemoryTypes; }             \
            inline nk::cstr to_cstr(const nk::MemoryTypeValue value) {                    \
                switch (value) {                                                          \
                    case MemoryType::None:                                                \
                        return "None";                                                    \
                        __VA_OPT__(NK_MAP(_NK_SWITCH_TO_STRING_MEMORY_TYPE, __VA_ARGS__)) \
                }                                                                         \
                return "Invalid";                                                         \
            }                                                                             \
        }                                                                                 \
    }

#define _NK_GET_REST(first, ...) __VA_ARGS__ __VA_OPT__(, )

#define NK_EXTEND_MEMORY_TYPE(...)                                             \
    __VA_OPT__(                                                                \
        namespace nk::MemoryType {                                             \
            enum {                                                             \
                NK_1ST_ARGUMENT(__VA_ARGS__) = OriginalMaxMemoryTypes + 1,     \
                _NK_GET_REST(__VA_ARGS__)                                      \
                    MaxMemoryTypes,                                            \
            };                                                                 \
            inline constexpr nk::u32 extended_max() { return MaxMemoryTypes; } \
            inline nk::cstr extended_to_cstr(nk::MemoryTypeValue value) {      \
                switch (value) {                                               \
                    NK_MAP(_NK_SWITCH_TO_STRING_MEMORY_TYPE, __VA_ARGS__)      \
                }                                                              \
                return "Invalid";                                              \
            }                                                                  \
        })


_NK_DEFINE_MEMORY_TYPE(System, App)

namespace nk {
    void memory_system_expanded_memory_type(const u64 max_memory_type, const std::function<cstr(MemoryTypeValue)>& memory_type_to_cstr);
}

#define NK_MEMORY_SYSTEM_EXPANDED_MEMORY_TYPE() \
    nk::memory_system_expanded_memory_type(nk::MemoryType::extended_max(), nk::MemoryType::extended_to_cstr)

#else

#define NK_EXTEND_MEMORY_TYPE(...)

#define NK_MEMORY_SYSTEM_EXPANDED_MEMORY_TYPE()

#endif
