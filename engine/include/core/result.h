#pragma once

#include <concepts>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

namespace nk {
    namespace result_detail {
        template <typename T>
        struct success {
            T value;
        };

        struct void_success {};

        template <typename E>
        struct failure {
            E error;
        };

        [[noreturn]] inline void bad_access() noexcept {
            std::abort();
        }

        template <typename T>
        inline constexpr bool valid_alternative =
            std::is_object_v<T> &&
            !std::is_array_v<T> &&
            !std::is_const_v<T> &&
            !std::is_volatile_v<T> &&
            std::is_destructible_v<T>;

        template <typename T, typename E>
        inline constexpr bool trivially_copyable_alternatives =
            std::is_trivially_copyable_v<T> &&
            std::is_trivially_copyable_v<E>;
    }

    template <typename T>
    [[nodiscard]] constexpr auto ok(T&& value)
        noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T&&>) {
        return result_detail::success<std::decay_t<T>>{
            std::forward<T>(value)};
    }

    [[nodiscard]] constexpr result_detail::void_success ok() noexcept {
        return {};
    }

    template <typename E>
    [[nodiscard]] constexpr auto err(E&& error)
        noexcept(std::is_nothrow_constructible_v<std::decay_t<E>, E&&>) {
        return result_detail::failure<std::decay_t<E>>{
            std::forward<E>(error)};
    }

    template <typename T, typename E>
    class [[nodiscard]] result {
        static_assert(
            result_detail::valid_alternative<T>,
            "nk::result value must be a mutable, destructible, non-array object");
        static_assert(
            result_detail::valid_alternative<E>,
            "nk::result error must be a mutable, destructible, non-array object");

        static constexpr bool trivial_destructor =
            std::is_trivially_destructible_v<T> &&
            std::is_trivially_destructible_v<E>;
        static constexpr bool trivial_copy =
            result_detail::trivially_copyable_alternatives<T, E>;

    public:
        result() = delete;

        template <typename U>
            requires std::constructible_from<T, U&&>
        constexpr result(result_detail::success<U>&& success)
            noexcept(std::is_nothrow_constructible_v<T, U&&>)
            : m_value(std::forward<U>(success.value)),
              m_has_value(true) {}

        template <typename G>
            requires std::constructible_from<E, G&&>
        constexpr result(result_detail::failure<G>&& failure)
            noexcept(std::is_nothrow_constructible_v<E, G&&>)
            : m_error(std::forward<G>(failure.error)),
              m_has_value(false) {}

        constexpr result(const result& other)
            requires trivial_copy = default;

        constexpr result(const result& other)
            noexcept(
                std::is_nothrow_copy_constructible_v<T> &&
                std::is_nothrow_copy_constructible_v<E>)
            requires (
                std::copy_constructible<T> &&
                std::copy_constructible<E> &&
                !trivial_copy)
            : m_has_value(other.m_has_value) {
            _copy_active(other);
        }

        result(const result&)
            requires (!std::copy_constructible<T> || !std::copy_constructible<E>) = delete;

        constexpr result(result&& other)
            requires trivial_copy = default;

        constexpr result(result&& other)
            noexcept(
                std::is_nothrow_move_constructible_v<T> &&
                std::is_nothrow_move_constructible_v<E>)
            requires (
                std::move_constructible<T> &&
                std::move_constructible<E> &&
                !trivial_copy)
            : m_has_value(other.m_has_value) {
            _move_active(other);
        }

        result(result&&)
            requires (!std::move_constructible<T> || !std::move_constructible<E>) = delete;

        constexpr result& operator=(const result& other)
            requires trivial_copy = default;

        constexpr result& operator=(const result& other)
            noexcept(
                std::is_nothrow_destructible_v<T> &&
                std::is_nothrow_destructible_v<E> &&
                std::is_nothrow_copy_constructible_v<T> &&
                std::is_nothrow_copy_constructible_v<E>)
            requires (
                std::copy_constructible<T> &&
                std::copy_constructible<E> &&
                !trivial_copy) {
            if (this == std::addressof(other))
                return *this;
            _destroy_active();
            m_has_value = other.m_has_value;
            _copy_active(other);
            return *this;
        }

        result& operator=(const result&)
            requires (!std::copy_constructible<T> || !std::copy_constructible<E>) = delete;

        constexpr result& operator=(result&& other)
            requires trivial_copy = default;

        constexpr result& operator=(result&& other)
            noexcept(
                std::is_nothrow_destructible_v<T> &&
                std::is_nothrow_destructible_v<E> &&
                std::is_nothrow_move_constructible_v<T> &&
                std::is_nothrow_move_constructible_v<E>)
            requires (
                std::move_constructible<T> &&
                std::move_constructible<E> &&
                !trivial_copy) {
            if (this == std::addressof(other))
                return *this;
            _destroy_active();
            m_has_value = other.m_has_value;
            _move_active(other);
            return *this;
        }

        result& operator=(result&&)
            requires (!std::move_constructible<T> || !std::move_constructible<E>) = delete;

        constexpr ~result()
            requires trivial_destructor = default;

        constexpr ~result()
            noexcept(
                std::is_nothrow_destructible_v<T> &&
                std::is_nothrow_destructible_v<E>)
            requires (!trivial_destructor) {
            _destroy_active();
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return m_has_value;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return m_has_value;
        }

        constexpr T& value() & noexcept {
            if (!m_has_value)
                result_detail::bad_access();
            return m_value;
        }

        constexpr const T& value() const& noexcept {
            if (!m_has_value)
                result_detail::bad_access();
            return m_value;
        }

        constexpr T&& value() && noexcept {
            if (!m_has_value)
                result_detail::bad_access();
            return std::move(m_value);
        }

        constexpr E& error() & noexcept {
            if (m_has_value)
                result_detail::bad_access();
            return m_error;
        }

        constexpr const E& error() const& noexcept {
            if (m_has_value)
                result_detail::bad_access();
            return m_error;
        }

        constexpr E&& error() && noexcept {
            if (m_has_value)
                result_detail::bad_access();
            return std::move(m_error);
        }

        constexpr T& operator*() & noexcept { return value(); }
        constexpr const T& operator*() const& noexcept { return value(); }
        constexpr T&& operator*() && noexcept { return std::move(*this).value(); }

        constexpr T* operator->() noexcept { return std::addressof(value()); }
        constexpr const T* operator->() const noexcept {
            return std::addressof(value());
        }

        template <typename U>
            requires (
                std::copy_constructible<T> &&
                std::constructible_from<T, U&&>)
        [[nodiscard]] constexpr T value_or(U&& fallback) const&
            noexcept(
                std::is_nothrow_copy_constructible_v<T> &&
                std::is_nothrow_constructible_v<T, U&&>) {
            if (m_has_value)
                return m_value;
            return T(std::forward<U>(fallback));
        }

        template <typename U>
            requires (
                std::move_constructible<T> &&
                std::constructible_from<T, U&&>)
        [[nodiscard]] constexpr T value_or(U&& fallback) &&
            noexcept(
                std::is_nothrow_move_constructible_v<T> &&
                std::is_nothrow_constructible_v<T, U&&>) {
            if (m_has_value)
                return std::move(m_value);
            return T(std::forward<U>(fallback));
        }

    private:
        constexpr void _copy_active(const result& other) {
            if (m_has_value)
                std::construct_at(std::addressof(m_value), other.m_value);
            else
                std::construct_at(std::addressof(m_error), other.m_error);
        }

        constexpr void _move_active(result& other) {
            if (m_has_value) {
                std::construct_at(
                    std::addressof(m_value),
                    std::move(other.m_value));
            } else {
                std::construct_at(
                    std::addressof(m_error),
                    std::move(other.m_error));
            }
        }

        constexpr void _destroy_active()
            noexcept(
                std::is_nothrow_destructible_v<T> &&
                std::is_nothrow_destructible_v<E>) {
            if (m_has_value)
                std::destroy_at(std::addressof(m_value));
            else
                std::destroy_at(std::addressof(m_error));
        }

        union {
            T m_value;
            E m_error;
        };
        bool m_has_value;
    };

    template <typename E>
    class [[nodiscard]] result<void, E> {
        static_assert(
            result_detail::valid_alternative<E>,
            "nk::result error must be a mutable, destructible, non-array object");

        static constexpr bool trivial_destructor =
            std::is_trivially_destructible_v<E>;
        static constexpr bool trivial_copy =
            std::is_trivially_copyable_v<E>;

    public:
        result() = delete;

        constexpr result(result_detail::void_success) noexcept
            : m_success{},
              m_has_value(true) {}

        template <typename G>
            requires std::constructible_from<E, G&&>
        constexpr result(result_detail::failure<G>&& failure)
            noexcept(std::is_nothrow_constructible_v<E, G&&>)
            : m_error(std::forward<G>(failure.error)),
              m_has_value(false) {}

        constexpr result(const result& other)
            requires trivial_copy = default;

        constexpr result(const result& other)
            noexcept(std::is_nothrow_copy_constructible_v<E>)
            requires (std::copy_constructible<E> && !trivial_copy)
            : m_has_value(other.m_has_value) {
            if (!m_has_value)
                std::construct_at(std::addressof(m_error), other.m_error);
        }

        result(const result&)
            requires (!std::copy_constructible<E>) = delete;

        constexpr result(result&& other)
            requires trivial_copy = default;

        constexpr result(result&& other)
            noexcept(std::is_nothrow_move_constructible_v<E>)
            requires (std::move_constructible<E> && !trivial_copy)
            : m_has_value(other.m_has_value) {
            if (!m_has_value) {
                std::construct_at(
                    std::addressof(m_error),
                    std::move(other.m_error));
            }
        }

        result(result&&)
            requires (!std::move_constructible<E>) = delete;

        constexpr result& operator=(const result& other)
            requires trivial_copy = default;

        constexpr result& operator=(const result& other)
            noexcept(
                std::is_nothrow_destructible_v<E> &&
                std::is_nothrow_copy_constructible_v<E>)
            requires (std::copy_constructible<E> && !trivial_copy) {
            if (this == std::addressof(other))
                return *this;
            if (!m_has_value)
                std::destroy_at(std::addressof(m_error));
            m_has_value = other.m_has_value;
            if (!m_has_value)
                std::construct_at(std::addressof(m_error), other.m_error);
            return *this;
        }

        result& operator=(const result&)
            requires (!std::copy_constructible<E>) = delete;

        constexpr result& operator=(result&& other)
            requires trivial_copy = default;

        constexpr result& operator=(result&& other)
            noexcept(
                std::is_nothrow_destructible_v<E> &&
                std::is_nothrow_move_constructible_v<E>)
            requires (std::move_constructible<E> && !trivial_copy) {
            if (this == std::addressof(other))
                return *this;
            if (!m_has_value)
                std::destroy_at(std::addressof(m_error));
            m_has_value = other.m_has_value;
            if (!m_has_value) {
                std::construct_at(
                    std::addressof(m_error),
                    std::move(other.m_error));
            }
            return *this;
        }

        result& operator=(result&&)
            requires (!std::move_constructible<E>) = delete;

        constexpr ~result()
            requires trivial_destructor = default;

        constexpr ~result()
            noexcept(std::is_nothrow_destructible_v<E>)
            requires (!trivial_destructor) {
            if (!m_has_value)
                std::destroy_at(std::addressof(m_error));
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return m_has_value;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return m_has_value;
        }

        constexpr void value() const noexcept {
            if (!m_has_value)
                result_detail::bad_access();
        }

        constexpr E& error() & noexcept {
            if (m_has_value)
                result_detail::bad_access();
            return m_error;
        }

        constexpr const E& error() const& noexcept {
            if (m_has_value)
                result_detail::bad_access();
            return m_error;
        }

        constexpr E&& error() && noexcept {
            if (m_has_value)
                result_detail::bad_access();
            return std::move(m_error);
        }

    private:
        union {
            result_detail::void_success m_success;
            E m_error;
        };
        bool m_has_value;
    };
}
