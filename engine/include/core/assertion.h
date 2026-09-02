#pragma once

#include "core/format.h"

#if NK_DEV_MODE <= NK_DEBUG
    #define NK_ENABLE_ASSERT TRUE
#endif

#if NK_ENABLE_ASSERT == TRUE

namespace nk {
    inline void write_assert_failure(
        const strview expression,
        const strview file,
        const u32 line,
        const strview message = {}) noexcept {
        strbuf<4096> output;
        if (message.empty())
            format_to(output, "{} > Failed at {}:{}", expression, file, line);
        else
            format_to(output, "{} > Failed at {}:{}: '{}'", expression, file, line, message);
        output.mark_truncated();

        constexpr cstr style_text = "\033[38;2;255;255;255;48;2;145;23;23m";
        constexpr cstr reset_text = "\n\033[0m";
        constexpr strview style{
            style_text,
            sizeof("\033[38;2;255;255;255;48;2;145;23;23m") - 1};
        constexpr strview reset{reset_text, sizeof("\n\033[0m") - 1};
        os::write(style.data(), style.length());
        os::write(output.data(), output.length());
        os::write(reset.data(), reset.length());
        os::flush();
    }

    template <typename... Args>
    inline void report_assert_failure(
        const cstr expression,
        const cstr file,
        const u32 line,
        format_string<std::type_identity_t<Args>...> format,
        Args&&... args) noexcept {
        strbuf<2048> message;
        format_to(message, format, std::forward<Args>(args)...);
        message.mark_truncated();
        write_assert_failure(expression, file, line, message.view());
    }

    inline void report_assert_failure(
        const cstr expression,
        const cstr file,
        const u32 line) noexcept {
        write_assert_failure(expression, file, line);
    }
}

    #define NK_ASSERT_3(expression, fmt, ...)                                                 \
        do {                                                                                  \
            if (!(expression)) {                                                              \
                nk::report_assert_failure(#expression, __FILE__, __LINE__, fmt, __VA_ARGS__); \
                nk::os::debug_break();                                                        \
            }                                                                                 \
        } while (false)

    #define NK_ASSERT_2(expression, msg)                                         \
        do {                                                                     \
            if (!(expression)) {                                                 \
                nk::report_assert_failure(#expression, __FILE__, __LINE__, msg); \
                nk::os::debug_break();                                           \
            }                                                                    \
        } while (false)

    #define NK_ASSERT_1(expression)                                         \
        do {                                                                \
            if (!(expression)) {                                            \
                nk::report_assert_failure(#expression, __FILE__, __LINE__); \
                nk::os::debug_break();                                      \
            }                                                               \
        } while (false)

    #define _NK_ASSERT_FUNC_CHOOSER(_f1, _f2, _f3, _f4, ...)  _f4
    #define _NK_ASSERT_FUNC_RECOMPOSER(args_with_parenthesis) _NK_ASSERT_FUNC_CHOOSER args_with_parenthesis
    #define _NK_ASSERT_CHOOSE_FROM_ARG_COUNT(...)             _NK_ASSERT_FUNC_RECOMPOSER((__VA_ARGS__, NK_ASSERT_3, NK_ASSERT_2, NK_ASSERT_1, ))
    #define _NK_ASSERT_NO_ARG_EXPANDER()                      , , , NK_ASSERT_UNDEFINED
    #define _NK_ASSERT_MACRO_CHOOSER(...)                     _NK_ASSERT_CHOOSE_FROM_ARG_COUNT(_NK_ASSERT_NO_ARG_EXPANDER __VA_ARGS__())

    #define Assert(...)                                       _NK_ASSERT_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)
    #define AssertKeep(...)                                   Assert(__VA_ARGS__)

#else

    #define Assert(...)
    #define AssertKeep(...) NK_1ST_ARGUMENT(__VA_ARGS__)

#endif
