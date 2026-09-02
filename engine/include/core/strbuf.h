#pragma once

#include <cstddef>
#include <cstring>

#include "core/strview.h"

namespace nk {
    template <u64 Capacity>
    class strbuf {
    public:
        static_assert(
            Capacity < numeric::u64_max,
            "nk::strbuf capacity must leave room for the null terminator");

        constexpr strbuf() noexcept = default;

        strbuf(strview text) noexcept {
            append(text);
        }

        template <std::size_t Length>
        strbuf(const char (&text)[Length]) noexcept {
            append(strview{text});
        }

        bool assign(const strview text) noexcept {
            const u64 copied = text.length() < Capacity
                ? text.length()
                : Capacity;
            if (copied != 0)
                std::memmove(m_data, text.data(), copied);
            m_length = copied;
            m_truncated = copied != text.length();
            m_data[m_length] = '\0';
            return !m_truncated;
        }

        bool append(const strview text) noexcept {
            const u64 available = Capacity - m_length;
            const u64 copied = text.length() < available
                ? text.length()
                : available;

            if (copied != 0)
                std::memmove(m_data + m_length, text.data(), copied);

            m_length += copied;
            m_data[m_length] = '\0';

            if (copied != text.length())
                m_truncated = true;
            return copied == text.length();
        }

        bool append(const char character) noexcept {
            if (m_length == Capacity) {
                m_truncated = true;
                return false;
            }

            m_data[m_length++] = character;
            m_data[m_length] = '\0';
            return true;
        }

        void clear() noexcept {
            m_length = 0;
            m_truncated = false;
            m_data[0] = '\0';
        }

        void mark_truncated(
            const strview marker = strview{"...<truncated>"}) noexcept {
            if (!m_truncated || Capacity == 0)
                return;

            const u64 marker_length = marker.length() < Capacity
                ? marker.length()
                : Capacity;
            const u64 offset = Capacity - marker_length;
            if (marker_length != 0)
                std::memmove(m_data + offset, marker.data(), marker_length);
            m_length = Capacity;
            m_data[m_length] = '\0';
        }

        char* data() noexcept { return m_data; }
        const char* data() const noexcept { return m_data; }
        nk::cstr cstr() const noexcept { return m_data; }
        constexpr u64 capacity() const noexcept { return Capacity; }
        constexpr u64 length() const noexcept { return m_length; }
        constexpr bool empty() const noexcept { return m_length == 0; }
        constexpr bool truncated() const noexcept { return m_truncated; }
        strview view() const noexcept { return {m_data, m_length}; }
        explicit operator strview() const noexcept { return view(); }

    private:
        char m_data[Capacity + 1]{};
        u64 m_length = 0;
        bool m_truncated = false;
    };

    template <u64 Capacity>
    constexpr strview::strview(const strbuf<Capacity>& text) noexcept
        : m_data{text.data()},
          m_length{text.length()} {}
}
