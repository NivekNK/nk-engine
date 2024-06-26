#pragma once

#if defined(NK_DEBUG)
    #define NK_ENABLE_ASSERT TRUE
#endif

#if NK_ENABLE_ASSERT

namespace nk {
    template <typename... Args>
    static void report_assert_failure_message(
        const cstr expression, const cstr file, const u32 line, const cstr fmt, Args&&... args) {
        str message;
        std::vformat_to(
            std::back_inserter(message),
            fmt,
            std::make_format_args(std::forward<Args>(args)...));
        FatalLog("{} > Failed '{}'", message, expression);
    }

    template <typename... Args>
    static void report_assert_failure(
        const cstr expression, const cstr file, const u32 line) {
        FatalLog("Failed {}", expression);
    }

    static void debug_break() {
#if defined(NK_PLATFORM_LINUX)
        __builtin_trap();
#elif defined(NK_PLATFORM_WINDOWS)
        __debugbreak();
#else
        FatalLog("nk-engine only supports Windows and Linux!");
#endif
    }
}

    #define AssertMsg(expr, fmt, ...)                                                                                    \
        {                                                                                                                \
            if (expr) {                                                                                                  \
            } else {                                                                                                     \
                nk::report_assert_failure_message(#expr, __FILE__, __LINE__, fmt __VA_OPT__(, ) __VA_ARGS__); \
                nk::debug_break();                                                                            \
            }                                                                                                            \
        }

    #define Assert(expr)                                                         \
        {                                                                        \
            if (expr) {                                                          \
            } else {                                                             \
                nk::report_assert_failure(#expr, __FILE__, __LINE__); \
                nk::debug_break();                                    \
            }                                                                    \
        }

    #define AssertKeep(expr)              Assert(expr)

    #define AssertKeepMsg(expr, fmt, ...) AssertMsg(expr, fmt __VA_OPT__(, ) __VA_ARGS__)

#else

    #define Assert(expr)
    #define AssertMsg(expr, message)
    #define AssertKeep(expr)             expr
    #define AssertKeepMsg(expr, message) expr

#endif
