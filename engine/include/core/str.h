#pragma once

#include "core/strview.h"

namespace nk {
    namespace mem {
        class Allocator;
    }

    class str {
    public:
        static constexpr u64 inline_capacity = 23;

        str() = delete;
        explicit str(mem::Allocator& allocator) noexcept;
        str(mem::Allocator& allocator, strview text);
        str(mem::Allocator& allocator, nk::cstr text);
        str(mem::Allocator& allocator, const str& other);

        str(const str& other);
        str(str&& other) noexcept;
        str& operator=(const str& other);
        str& operator=(str&& other) noexcept;
        ~str();

        bool assign(strview text) noexcept;
        bool assign(nk::cstr text) noexcept;
        bool append(strview text) noexcept;
        bool append(nk::cstr text) noexcept;
        bool append(char character) noexcept;
        bool reserve(u64 minimum_capacity) noexcept;
        void clear() noexcept;

        char* data() noexcept { return _data(); }
        const char* data() const noexcept { return _data(); }
        nk::cstr cstr() const noexcept { return _data(); }
        u64 length() const noexcept { return m_length; }
        u64 capacity() const noexcept { return m_capacity; }
        bool empty() const noexcept { return m_length == 0; }
        mem::Allocator* allocator() noexcept { return m_allocator; }
        const mem::Allocator* allocator() const noexcept { return m_allocator; }
        strview view() const noexcept { return {_data(), m_length}; }

        str clone(mem::Allocator& allocator) const;

        char& operator[](u64 index) noexcept { return _data()[index]; }
        const char& operator[](u64 index) const noexcept { return _data()[index]; }
        explicit operator strview() const noexcept { return view(); }

        bool operator==(strview other) const noexcept {
            return view() == other;
        }

        bool operator!=(strview other) const noexcept {
            return !(*this == other);
        }

    private:
        union Storage {
            constexpr Storage() noexcept
                : inline_data{} {}

            char* heap_data;
            char inline_data[inline_capacity + 1];
        };

        static void _diagnostic(nk::cstr message) noexcept;
        [[noreturn]] static void _fatal(nk::cstr message) noexcept;

        bool _is_inline() const noexcept {
            return m_capacity == inline_capacity;
        }
        char* _data() noexcept {
            return _is_inline() ? m_storage.inline_data : m_storage.heap_data;
        }
        const char* _data() const noexcept {
            return _is_inline() ? m_storage.inline_data : m_storage.heap_data;
        }

        bool _ensure_capacity(u64 minimum_capacity, bool grow) noexcept;
        bool _release_heap() noexcept;
        void _move_construct(str& other) noexcept;

        mem::Allocator* m_allocator;
        u64 m_length;
        u64 m_capacity;
        Storage m_storage;
    };

    inline bool operator==(const strview left, const str& right) noexcept {
        return left == right.view();
    }

    inline bool operator!=(const strview left, const str& right) noexcept {
        return !(left == right);
    }
}
