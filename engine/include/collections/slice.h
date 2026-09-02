#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>

#include "core/assertion.h"
#include "core/defines.h"

namespace nk::cl {
    template <typename T>
    class slice {
    public:
        static constexpr u64 npos = numeric::u64_max;

        constexpr slice() noexcept = default;

        template <typename U>
            requires std::is_convertible_v<U (*)[], T (*)[]>
        constexpr slice(U* data, const u64 length) noexcept
            : m_data{data},
              m_length{data == nullptr ? 0 : length} {}

        template <typename U, std::size_t Length>
            requires std::is_convertible_v<U (*)[], T (*)[]>
        constexpr slice(U (&values)[Length]) noexcept
            : m_data{values},
              m_length{Length} {}

        template <typename U>
            requires std::is_convertible_v<U (*)[], T (*)[]>
        constexpr slice(const slice<U>& other) noexcept
            : m_data{other.data()},
              m_length{other.length()} {}

        template <typename Container>
            requires requires(Container& container) {
                { container.data() } -> std::convertible_to<T*>;
                { container.length() } -> std::same_as<u64>;
            }
        constexpr explicit slice(Container& container) noexcept
            : m_data{container.data()},
              m_length{container.length()} {}

        constexpr T& operator[](const u64 index) const noexcept {
            Assert(index < m_length);
            return m_data[index];
        }

        constexpr T& at(const u64 index) const noexcept {
            Assert(index < m_length);
            return m_data[index];
        }

        constexpr T& first() const noexcept {
            Assert(m_length != 0);
            return m_data[0];
        }

        constexpr T& last() const noexcept {
            Assert(m_length != 0);
            return m_data[m_length - 1];
        }

        constexpr slice subslice(
            const u64 offset,
            const u64 count = npos) const noexcept {
            if (offset >= m_length)
                return {};
            const u64 available = m_length - offset;
            const u64 selected = count == npos || count > available
                ? available
                : count;
            return {m_data + offset, selected};
        }

        constexpr T* data() const noexcept { return m_data; }
        constexpr u64 length() const noexcept { return m_length; }
        constexpr bool empty() const noexcept { return m_length == 0; }
        constexpr T* begin() const noexcept { return m_data; }
        constexpr T* end() const noexcept {
            return m_data == nullptr ? nullptr : m_data + m_length;
        }

    private:
        T* m_data = nullptr;
        u64 m_length = 0;
    };
}
