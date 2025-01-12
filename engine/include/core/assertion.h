#pragma once

#if NK_DEV_MODE <= NK_DEBUG
    #define NK_ENABLE_ASSERT TRUE
#endif

#if NK_ENABLE_ASSERT == TRUE

namespace nk {
    void report_assert_failure(cstr expression, cstr file, u32 line) {
        std::string buffer = std::format("{} > Failed at {}:{}", expression, file, line);
        os::write("\033[38;2;255;255;255;48;2;145;23;23m", 34);
        os::write(buffer.c_str(), buffer.size());
        os::write("\n\033[0m", 5);
        os::flush();
    }

    void report_assert_failure(cstr expression, cstr file, u32 line, std::string_view msg) {
        std::string buffer = std::format("{} > Failed at {}:{}: '{}'", expression, file, line, msg);
        os::write("\033[38;2;255;255;255;48;2;145;23;23m", 34);
        os::write(buffer.c_str(), buffer.size());
        os::write("\n\033[0m", 5);
        os::flush();
    }

    template <typename... Args>
    void report_assert_failure(cstr expression, cstr file, u32 line, std::string_view fmt, Args&&... args) {
        std::string msg;
        std::vformat_to(std::back_inserter(buffer), fmt, std::make_format_args(args...));
        std::string buffer = std::format("{} > Failed at {}:{}: '{}'", expression, file, line, msg);
        os::write("\033[38;2;255;255;255;48;2;145;23;23m", 34);
        os::write(buffer.c_str(), buffer.size());
        os::write("\n\033[0m", 5);
        os::flush();
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
