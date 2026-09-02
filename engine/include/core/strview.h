#pragma once

#include <cstddef>
#include <type_traits>

#include "core/defines.h"

namespace nk {
    class str;

    template <u64 Capacity>
    class strbuf;

    class strview {
    public:
        static constexpr u64 npos = numeric::u64_max;

        constexpr strview() noexcept = default;

        template <std::size_t Length>
        constexpr strview(const char (&text)[Length]) noexcept
            : m_data{text},
              m_length{Length == 0 ? 0 : Length - 1} {}

        strview(nk::cstr text) noexcept;

        constexpr strview(const char* data, const u64 length) noexcept
            : m_data{data},
              m_length{data == nullptr ? 0 : length} {}

        strview(const str& text) noexcept;

        template <u64 Capacity>
        constexpr strview(const strbuf<Capacity>& text) noexcept;

        constexpr const char* data() const noexcept { return m_data; }
        constexpr u64 length() const noexcept { return m_length; }
        constexpr bool empty() const noexcept { return m_length == 0; }

        constexpr const char& operator[](const u64 index) const noexcept {
            return m_data[index];
        }

        constexpr const char* begin() const noexcept { return m_data; }
        constexpr const char* end() const noexcept {
            return m_data == nullptr ? nullptr : m_data + m_length;
        }

        i32 compare(strview other) const noexcept;
        bool starts_with(strview prefix) const noexcept;
        bool ends_with(strview suffix) const noexcept;
        u64 find(char character, u64 offset = 0) const noexcept;
        u64 find(strview text, u64 offset = 0) const noexcept;
        strview substr(u64 offset, u64 count = npos) const noexcept;

    private:
        const char* m_data = nullptr;
        u64 m_length = 0;
    };

    inline bool operator==(const strview left, const strview right) noexcept {
        return left.compare(right) == 0;
    }

    inline bool operator!=(const strview left, const strview right) noexcept {
        return !(left == right);
    }
}
