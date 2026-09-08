#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "core/defines.h"
#include "core/result.h"
#include "memory/allocator.h"
#include "memory/object_lifetime.h"

namespace nk::cl {
    enum class ring_error : u8 {
        invalid_state,
        invalid_capacity,
        out_of_memory,
        full,
        empty,
        release_failed,
    };

    // Fixed-capacity FIFO. Synchronization is intentionally external so the
    // queue can be used without lock overhead in single-threaded code.
    template <mem::ObjectStorageType T>
    class ring final {
    public:
        ring() noexcept = default;

        ring(const ring&) = delete;
        ring& operator=(const ring&) = delete;
        ring(ring&&) = delete;
        ring& operator=(ring&&) = delete;

        ~ring() noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T> {
            (void)_shutdown({__FILE__, __LINE__});
        }

        [[nodiscard]] result<void, ring_error> _ring_init(
            mem::Allocator* allocator,
            u64 capacity)
            requires std::is_destructible_v<T> {
            return _init({__FILE__, __LINE__}, allocator, capacity);
        }

#if NK_MEMORY_TRACKING_ENABLED
        [[nodiscard]] result<void, ring_error> _ring_init(
            cstr file,
            u32 line,
            mem::Allocator* allocator,
            u64 capacity)
            requires std::is_destructible_v<T> {
            return _init({file, line}, allocator, capacity);
        }
#endif

        template <typename... Args>
            requires std::is_constructible_v<T, Args...>
        [[nodiscard]] result<void, ring_error> emplace(Args&&... args) {
            if (m_data == nullptr)
                return err(ring_error::invalid_state);
            if (full())
                return err(ring_error::full);

            (void)mem::construct_object(
                m_data + m_tail,
                std::forward<Args>(args)...);
            m_tail = _next(m_tail);
            ++m_count;
            return ok();
        }

        [[nodiscard]] result<void, ring_error> push(T&& value)
            requires std::is_move_constructible_v<T> {
            return emplace(std::move(value));
        }

        [[nodiscard]] result<void, ring_error> push_copy(const T& value)
            requires std::is_copy_constructible_v<T> {
            return emplace(value);
        }

        [[nodiscard]] result<T, ring_error> pop()
            requires std::is_move_constructible_v<T> &&
                     std::is_destructible_v<T> {
            if (m_data == nullptr)
                return err(ring_error::invalid_state);
            if (empty())
                return err(ring_error::empty);

            T value{std::move(m_data[m_head])};
            std::destroy_at(m_data + m_head);
            m_head = _next(m_head);
            --m_count;
            return ok(std::move(value));
        }

        [[nodiscard]] T* front() noexcept {
            return empty() ? nullptr : m_data + m_head;
        }

        [[nodiscard]] const T* front() const noexcept {
            return empty() ? nullptr : m_data + m_head;
        }

        void clear() noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T> {
            while (m_count > 0) {
                std::destroy_at(m_data + m_head);
                m_head = _next(m_head);
                --m_count;
            }
            m_head = 0;
            m_tail = 0;
        }

        [[nodiscard]] result<void, ring_error> _ring_shutdown()
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T> {
            return _shutdown({__FILE__, __LINE__});
        }

#if NK_MEMORY_TRACKING_ENABLED
        [[nodiscard]] result<void, ring_error> _ring_shutdown(
            cstr file,
            u32 line)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T> {
            return _shutdown({file, line});
        }
#endif

        [[nodiscard]] u64 length() const noexcept { return m_count; }
        [[nodiscard]] u64 capacity() const noexcept { return m_capacity; }
        [[nodiscard]] bool empty() const noexcept { return m_count == 0; }
        [[nodiscard]] bool full() const noexcept {
            return m_capacity > 0 && m_count == m_capacity;
        }
        [[nodiscard]] mem::Allocator* allocator() noexcept {
            return m_allocator;
        }
        [[nodiscard]] const mem::Allocator* allocator() const noexcept {
            return m_allocator;
        }

    private:
        [[nodiscard]] result<void, ring_error> _init(
            mem::SourceLocation source,
            mem::Allocator* allocator,
            u64 capacity)
            requires std::is_destructible_v<T> {
            if (m_data != nullptr || m_allocator != nullptr)
                return err(ring_error::invalid_state);
            if (allocator == nullptr || capacity == 0 ||
                capacity > numeric::u64_max / sizeof(T)) {
                return err(ring_error::invalid_capacity);
            }

#if NK_MEMORY_TRACKING_ENABLED
            T* data = allocator->_allocate_lot_t<T>(
                source.file,
                source.line,
                capacity);
#else
            static_cast<void>(source);
            T* data = allocator->_allocate_lot_t<T>(capacity);
#endif
            if (data == nullptr)
                return err(ring_error::out_of_memory);

            m_data = data;
            m_allocator = allocator;
            m_capacity = capacity;
            return ok();
        }

        [[nodiscard]] result<void, ring_error> _shutdown(
            mem::SourceLocation source)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T> {
            if (m_data == nullptr) {
                if (m_allocator == nullptr && m_capacity == 0 && m_count == 0)
                    return ok();
                return err(ring_error::invalid_state);
            }

            clear();
#if NK_MEMORY_TRACKING_ENABLED
            const bool freed = m_allocator->_free_lot_t<T>(
                source.file,
                source.line,
                m_data,
                m_capacity);
#else
            static_cast<void>(source);
            const bool freed = m_allocator->_free_lot_t<T>(m_data, m_capacity);
#endif
            if (!freed)
                return err(ring_error::release_failed);

            m_data = nullptr;
            m_allocator = nullptr;
            m_capacity = 0;
            return ok();
        }

        [[nodiscard]] u64 _next(const u64 index) const noexcept {
            const u64 next = index + 1;
            return next == m_capacity ? 0 : next;
        }

        T* m_data = nullptr;
        mem::Allocator* m_allocator = nullptr;
        u64 m_capacity = 0;
        u64 m_head = 0;
        u64 m_tail = 0;
        u64 m_count = 0;
    };
}

#if NK_MEMORY_TRACKING_ENABLED
    #define ring_init(allocator, capacity) \
        _ring_init(__FILE__, __LINE__, (allocator), (capacity))
    #define ring_shutdown() \
        _ring_shutdown(__FILE__, __LINE__)
#else
    #define ring_init(allocator, capacity) \
        _ring_init((allocator), (capacity))
    #define ring_shutdown() \
        _ring_shutdown()
#endif
