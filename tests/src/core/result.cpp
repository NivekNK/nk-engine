#include <gtest/gtest.h>

#include "core/result.h"

namespace nktest {
    class Testing {
    public:
        Testing(nk::u32 value) : m_value{value} {}

        nk::u32 get_value() const { return m_value; }

    private:
        nk::u32 m_value;
    };

    class MyError {
    public:
        enum class Type {
            Unknown
        };

        MyError(Type type = Type::Unknown, std::string_view message = "")
            : m_type(type), m_message(message) {}

        Type get_type() const { return m_type; }
        std::string_view get_message() const { return m_message; }

    private:
        Type m_type;
        std::string_view m_message;
    };
}

nk::Result<nktest::Testing, nktest::MyError> test_err_maybe(bool error) {
    if (error) {
        return nktest::MyError(nktest::MyError::Type::Unknown, "My Message");
    }

    return nktest::Testing(5);
}

nk::Result<nktest::Testing, nktest::MyError> test_err_maybe() {
    auto error = test_err_maybe(true);
    return std::move(error);
}

TEST(Result, ResultCreation) {
    auto ok_value = test_err_maybe(false);

    match(ok_value,
        OK(ok) {
            std::cout << ok.get_value() << std::endl;
        },
        ERR(err) {
            std::cout << err.get_message() << std::endl;
        }
    );

    auto err_value = test_err_maybe();
    match(err_value,
        OK(ok) {
            std::cout << ok.get_value() << std::endl;
        },
        ERR(err) {
            std::cout << err.get_message() << std::endl;
        }
    );
}
