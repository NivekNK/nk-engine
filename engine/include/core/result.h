#pragma once

namespace nk {
    // Default Error Type
    class Err {
    public:
        enum class Type {
            Unknown
        };

        Err(Type type = Type::Unknown, std::string_view message = "")
            : type(type), message(message) {}

        Type get_type() const { return type; }
        std::string_view get_message() const { return message; }

    private:
        Type type;
        std::string_view message;
    };

    // Error Concept
    template <typename T>
    concept IErr = requires(T error) {
        { error.get_message() } -> std::convertible_to<std::string_view>;
        { error.get_type() };
    };

    // Ok Class
    template <typename T>
    class Ok {
    public:
        explicit constexpr Ok(T value) : value(std::move(value)) {}

        constexpr T&& take_value() && { return std::move(value); }
        constexpr const T& get_value() const { return value; }

    private:
        T value;
    };

    // Result Class with Default Error Type
    template <typename T, IErr E = Err>
    class Result {
    public:
        using VariantT = std::variant<Ok<T>, E>;

        // Constructor for Ok values (e.g., return 2)
        template <typename U>
        constexpr Result(U&& value)
            requires std::convertible_to<U, T>
            : variant(Ok<T>(std::forward<U>(value))) {}

        // Constructor for explicit Ok objects
        template <typename U>
        constexpr Result(Ok<U>&& value) : variant(std::move(value)) {}

        // Constructor for Error object
        constexpr Result(E&& error) : variant(std::move(error)) {}

        // Deleted copy constructor to enforce move-only semantics
        constexpr Result(const Result&) = delete;
        constexpr Result& operator=(const Result&) = delete;

        // Move constructor
        constexpr Result(Result&& other) noexcept : variant(std::move(other.variant)) {}

        // Move assignment operator
        constexpr Result& operator=(Result&& other) noexcept {
            variant = std::move(other.variant);
            return *this;
        }

        // Check if the result is Ok
        constexpr bool is_ok() const { return std::holds_alternative<Ok<T>>(variant); }

        // Check if the result is Err
        constexpr bool is_err() const { return std::holds_alternative<E>(variant); }

        // Get the Ok value
        constexpr const T& ok_value() const { return std::get<Ok<T>>(variant).get_value(); }

        // Public `take_ok_value()` for both lvalues and rvalues
        constexpr T&& take_ok_value() {
            return std::move(*this)._take_ok_value(); // Forward to rvalue-qualified version
        }

        // Get the Error
        constexpr const E& err_value() const { return std::get<E>(variant); }

    private:
        // Rvalue-qualified: Actual implementation for ownership transfer
        constexpr T&& _take_ok_value() && {
            return std::get<Ok<T>>(std::move(variant)).take_value();
        }

        VariantT variant;
    };
}

#define OK(ok)   auto& ok = _value.ok_value();
#define ERR(err) auto& err = _value.err_value();

#define match(result, ok_check, err_check) \
    do {                                   \
        auto& _value = result;             \
        if (_value.is_ok()) {              \
            ok_check                       \
        } else if (_value.is_err()) {      \
            err_check                      \
        }                                  \
    } while (false)
