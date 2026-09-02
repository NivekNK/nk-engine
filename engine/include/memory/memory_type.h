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
            struct Provider {                                                             \
                virtual ~Provider() = default;                                            \
                virtual Value max() const noexcept = 0;                                   \
                virtual cstr to_cstr(Value value) const noexcept = 0;                     \
            };                                                                            \
            namespace Internal {                                                          \
                inline const Provider* provider = nullptr;                                \
            }                                                                             \
            inline Value max() noexcept {                                                 \
                if (Internal::provider != nullptr)                                        \
                    return Internal::provider->max();                                     \
                return OriginalMaxMemoryTypes;                                            \
            }                                                                             \
            inline nk::cstr to_cstr(const Value value) noexcept {                         \
                if (Internal::provider != nullptr && value > OriginalMaxMemoryTypes)      \
                    return Internal::provider->to_cstr(value);                            \
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
                struct ExtendedProvider final : Provider {                                       \
                    Value max() const noexcept override { return MaxMemoryTypes; }                 \
                    nk::cstr to_cstr(const Value value) const noexcept override {                  \
                        switch (value) {                                                          \
                            NK_MAP(_NK_SWITCH_TO_STRING_MEMORY_TYPE, __VA_ARGS__)                 \
                        }                                                                         \
                        return "Invalid";                                                         \
                    }                                                                             \
                };                                                                                \
                inline const ExtendedProvider extended_provider{};                                \
            })

_NK_DEFINE_MEMORY_TYPE(Native, Test, System, Event, App, Renderer)

namespace nk {
    void memory_system_extended_memory_type(
        const MemoryType::Provider& provider) noexcept;
}

    #define NK_MEMORY_SYSTEM_EXTENDED_MEMORY_TYPE() \
        nk::memory_system_extended_memory_type(nk::MemoryType::extended_provider)

#else

    #define NK_EXTEND_MEMORY_TYPE(...)

    #define NK_MEMORY_SYSTEM_EXPANDED_MEMORY_TYPE()

#endif
