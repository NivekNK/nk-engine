#include "nkpch.h"

#include <cstdlib>

#include "core/str.h"
#include "memory/allocator.h"

namespace nk {
    namespace {
        char* allocate_text(mem::Allocator& allocator, const u64 count) noexcept {
#if NK_MEMORY_TRACKING_ENABLED
            return allocator._allocate_lot_t<char>(__FILE__, __LINE__, count);
#else
            return allocator._allocate_lot_t<char>(count);
#endif
        }

        bool free_text(
            mem::Allocator& allocator,
            char* data,
            const u64 count) noexcept {
#if NK_MEMORY_TRACKING_ENABLED
            return allocator._free_lot_t<char>(__FILE__, __LINE__, data, count);
#else
            return allocator._free_lot_t<char>(data, count);
#endif
        }
    }

    strview::strview(const nk::cstr text) noexcept
        : m_data{text},
          m_length{text == nullptr ? 0 : std::char_traits<char>::length(text)} {}

    strview::strview(const str& text) noexcept
        : m_data{text.data()},
          m_length{text.length()} {}

    i32 strview::compare(const strview other) const noexcept {
        const u64 common_length = m_length < other.m_length
            ? m_length
            : other.m_length;
        if (common_length != 0) {
            const i32 result = std::memcmp(m_data, other.m_data, common_length);
            if (result < 0)
                return -1;
            if (result > 0)
                return 1;
        }
        if (m_length < other.m_length)
            return -1;
        if (m_length > other.m_length)
            return 1;
        return 0;
    }

    bool strview::starts_with(const strview prefix) const noexcept {
        return prefix.m_length <= m_length &&
               (prefix.m_length == 0 ||
                std::memcmp(m_data, prefix.m_data, prefix.m_length) == 0);
    }

    bool strview::ends_with(const strview suffix) const noexcept {
        return suffix.m_length <= m_length &&
               (suffix.m_length == 0 ||
                std::memcmp(
                    m_data + m_length - suffix.m_length,
                    suffix.m_data,
                    suffix.m_length) == 0);
    }

    u64 strview::find(const char character, const u64 offset) const noexcept {
        if (offset >= m_length)
            return npos;
        const void* match = std::memchr(m_data + offset, character, m_length - offset);
        if (match == nullptr)
            return npos;
        return static_cast<const char*>(match) - m_data;
    }

    u64 strview::find(const strview text, const u64 offset) const noexcept {
        if (offset > m_length)
            return npos;
        if (text.m_length == 0)
            return offset;
        if (text.m_length > m_length - offset)
            return npos;

        const u64 last = m_length - text.m_length;
        for (u64 index = offset; index <= last; ++index) {
            if (m_data[index] == text.m_data[0] &&
                std::memcmp(m_data + index, text.m_data, text.m_length) == 0) {
                return index;
            }
        }
        return npos;
    }

    strview strview::substr(const u64 offset, const u64 count) const noexcept {
        if (offset >= m_length)
            return {};
        const u64 available = m_length - offset;
        const u64 selected = count == npos || count > available ? available : count;
        return {m_data + offset, selected};
    }

    str::str(mem::Allocator& allocator) noexcept
        : m_allocator{&allocator},
          m_length{0},
          m_capacity{inline_capacity},
          m_storage{} {
        m_storage.inline_data[0] = '\0';
    }

    str::str(mem::Allocator& allocator, const strview text)
        : str{allocator} {
        if (!assign(text))
            _fatal("nk::str construction failed.\n");
    }

    str::str(mem::Allocator& allocator, const nk::cstr text)
        : str{allocator, strview{text}} {}

    str::str(mem::Allocator& allocator, const str& other)
        : str{allocator, other.view()} {}

    str::str(const str& other)
        : m_allocator{other.m_allocator},
          m_length{0},
          m_capacity{inline_capacity},
          m_storage{} {
        m_storage.inline_data[0] = '\0';
        if (m_allocator == nullptr)
            _fatal("nk::str cannot copy a moved-from string.\n");
        if (!assign(other.view()))
            _fatal("nk::str copy construction failed.\n");
    }

    str::str(str&& other) noexcept
        : m_allocator{nullptr},
          m_length{0},
          m_capacity{inline_capacity},
          m_storage{} {
        _move_construct(other);
    }

    str& str::operator=(const str& other) {
        if (this != &other && !assign(other.view()))
            _fatal("nk::str copy assignment failed.\n");
        return *this;
    }

    str& str::operator=(str&& other) noexcept {
        if (this == &other)
            return *this;
        if (m_allocator == nullptr)
            _fatal("nk::str move assignment requires a destination allocator.\n");

        if (m_allocator == other.m_allocator) {
            if (!_release_heap())
                _fatal("nk::str failed to release storage during move assignment.\n");

            m_length = 0;
            m_capacity = inline_capacity;
            m_storage.inline_data[0] = '\0';

            if (!other._is_inline()) {
                m_length = other.m_length;
                m_capacity = other.m_capacity;
                m_storage.heap_data = other.m_storage.heap_data;
                other.m_length = 0;
                other.m_capacity = inline_capacity;
                other.m_storage.inline_data[0] = '\0';
            } else {
                m_length = other.m_length;
                std::memcpy(m_storage.inline_data, other.m_storage.inline_data, m_length + 1);
                other.clear();
            }
            return *this;
        }

        if (!assign(other.view()))
            _fatal("nk::str cross-allocator move assignment failed.\n");
        other.clear();
        return *this;
    }

    str::~str() {
        if (!_release_heap())
            _diagnostic("nk::str failed to release its storage.\n");
    }

    bool str::assign(const strview text) noexcept {
        u64 alias_offset = 0;
        const char* current = _data();
        const std::uintptr_t source_address =
            reinterpret_cast<std::uintptr_t>(text.data());
        const std::uintptr_t begin_address =
            reinterpret_cast<std::uintptr_t>(current);
        const std::uintptr_t end_address = begin_address + m_length;
        const bool aliases = text.data() != nullptr &&
            source_address >= begin_address &&
            source_address <= end_address;
        if (aliases)
            alias_offset = static_cast<u64>(text.data() - current);

        if (!_ensure_capacity(text.length(), false))
            return false;

        const char* source = aliases ? _data() + alias_offset : text.data();
        if (text.length() != 0)
            std::memmove(_data(), source, text.length());
        m_length = text.length();
        _data()[m_length] = '\0';
        return true;
    }

    bool str::assign(const nk::cstr text) noexcept {
        return assign(strview{text});
    }

    bool str::append(const strview text) noexcept {
        if (text.length() > numeric::u64_max - m_length)
            return false;
        const u64 requested_length = m_length + text.length();

        u64 alias_offset = 0;
        const char* current = _data();
        const std::uintptr_t source_address =
            reinterpret_cast<std::uintptr_t>(text.data());
        const std::uintptr_t begin_address =
            reinterpret_cast<std::uintptr_t>(current);
        const std::uintptr_t end_address = begin_address + m_length;
        const bool aliases = text.data() != nullptr &&
            source_address >= begin_address &&
            source_address <= end_address;
        if (aliases)
            alias_offset = static_cast<u64>(text.data() - current);

        if (!_ensure_capacity(requested_length, true))
            return false;

        const char* source = aliases ? _data() + alias_offset : text.data();
        if (text.length() != 0)
            std::memmove(_data() + m_length, source, text.length());
        m_length = requested_length;
        _data()[m_length] = '\0';
        return true;
    }

    bool str::append(const nk::cstr text) noexcept {
        return append(strview{text});
    }

    bool str::append(const char character) noexcept {
        return append(strview{&character, 1});
    }

    bool str::reserve(const u64 minimum_capacity) noexcept {
        return _ensure_capacity(minimum_capacity, false);
    }

    void str::clear() noexcept {
        m_length = 0;
        _data()[0] = '\0';
    }

    str str::clone(mem::Allocator& allocator) const {
        return {allocator, view()};
    }

    void str::_diagnostic(const nk::cstr message) noexcept {
        os::write(message, std::char_traits<char>::length(message));
        os::flush();
    }

    [[noreturn]] void str::_fatal(const nk::cstr message) noexcept {
        _diagnostic(message);
        std::abort();
    }

    bool str::_ensure_capacity(
        const u64 minimum_capacity,
        const bool grow) noexcept {
        if (minimum_capacity <= m_capacity)
            return true;
        if (m_allocator == nullptr || minimum_capacity == numeric::u64_max)
            return false;

        u64 next_capacity = minimum_capacity;
        if (grow && m_capacity <= (numeric::u64_max - 1) / 2) {
            const u64 doubled = m_capacity * 2;
            if (doubled > next_capacity)
                next_capacity = doubled;
        }
        if (next_capacity == numeric::u64_max)
            return false;

        char* replacement = allocate_text(*m_allocator, next_capacity + 1);
        if (replacement == nullptr)
            return false;

        std::memcpy(replacement, _data(), m_length + 1);
        if (!_is_inline()) {
            char* previous = m_storage.heap_data;
            const u64 previous_capacity = m_capacity;
            if (!free_text(*m_allocator, previous, previous_capacity + 1)) {
                (void)free_text(*m_allocator, replacement, next_capacity + 1);
                return false;
            }
        }

        m_storage.heap_data = replacement;
        m_capacity = next_capacity;
        return true;
    }

    bool str::_release_heap() noexcept {
        if (_is_inline())
            return true;
        if (m_allocator == nullptr || m_storage.heap_data == nullptr)
            return false;

        const bool released = free_text(
            *m_allocator,
            m_storage.heap_data,
            m_capacity + 1);
        if (released) {
            m_capacity = inline_capacity;
            m_length = 0;
            m_storage.inline_data[0] = '\0';
        }
        return released;
    }

    void str::_move_construct(str& other) noexcept {
        m_allocator = other.m_allocator;
        m_length = other.m_length;
        m_capacity = other.m_capacity;

        if (other._is_inline())
            std::memcpy(m_storage.inline_data, other.m_storage.inline_data, m_length + 1);
        else
            m_storage.heap_data = other.m_storage.heap_data;

        other.m_allocator = nullptr;
        other.m_length = 0;
        other.m_capacity = inline_capacity;
        other.m_storage.inline_data[0] = '\0';
    }
}
