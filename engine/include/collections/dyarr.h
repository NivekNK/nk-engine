#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "collections/arr_type.h"
#include "memory/allocator_owner.h"

namespace nk::cl {
    template <IArrT>
    class arr;

    template <IArrT T>
    class dyarr {
    public:
        dyarr() noexcept = default;

        dyarr(dyarr&& other) noexcept;
        dyarr& operator=(dyarr&& other)
            noexcept(std::is_nothrow_destructible_v<T>);

        dyarr(const dyarr&) = delete;
        dyarr& operator=(const dyarr&) = delete;

        ~dyarr() noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;

        T& operator[](u64 index);
        const T& operator[](u64 index) const;

        T& _dyarr_at(u64 index)
            requires std::is_default_constructible_v<T> &&
                     mem::RelocatableObject<T>;
#if NK_MEMORY_TRACKING_ENABLED
        T& _dyarr_at(cstr file, u32 line, u64 index)
            requires std::is_default_constructible_v<T> &&
                     mem::RelocatableObject<T>;
#endif

        const T& dyarr_at_const(u64 index) const;

        bool _dyarr_init(mem::Allocator* allocator, u64 capacity)
            requires std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_init(
            cstr file,
            u32 line,
            mem::Allocator* allocator,
            u64 capacity)
            requires std::is_destructible_v<T>;
#endif

        bool _dyarr_init_len(
            mem::Allocator* allocator,
            u64 capacity,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_init_len(
            cstr file,
            u32 line,
            mem::Allocator* allocator,
            u64 capacity,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
#endif

        bool _dyarr_init_own(
            mem::AllocatorOwner&& allocator_owner,
            u64 capacity)
            requires std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_init_own(
            cstr file,
            u32 line,
            mem::AllocatorOwner&& allocator_owner,
            u64 capacity)
            requires std::is_destructible_v<T>;
#endif

        bool _dyarr_init_own_len(
            mem::AllocatorOwner&& allocator_owner,
            u64 capacity,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_init_own_len(
            cstr file,
            u32 line,
            mem::AllocatorOwner&& allocator_owner,
            u64 capacity,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
#endif

        bool _dyarr_init_list(
            mem::Allocator* allocator,
            std::initializer_list<T> list)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_init_list(
            cstr file,
            u32 line,
            mem::Allocator* allocator,
            std::initializer_list<T> list)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
#endif

        bool _dyarr_init_list_own(
            mem::AllocatorOwner&& allocator_owner,
            std::initializer_list<T> list)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_init_list_own(
            cstr file,
            u32 line,
            mem::AllocatorOwner&& allocator_owner,
            std::initializer_list<T> list)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
#endif

        bool _dyarr_clear()
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_clear(cstr file, u32 line)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#endif

        bool _dyarr_shutdown()
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_shutdown(cstr file, u32 line)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#endif

        T& dyarr_first();
        const T& dyarr_first() const;

        T& dyarr_last();
        const T& dyarr_last() const;

        bool _dyarr_push(T& value)
            requires mem::RelocatableObject<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_push(cstr file, u32 line, T& value)
            requires mem::RelocatableObject<T>;
#endif

        bool _dyarr_push_ptr(T value)
            requires std::is_pointer_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_push_ptr(cstr file, u32 line, T value)
            requires std::is_pointer_v<T>;
#endif

        bool _dyarr_push_copy(const T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_copy_constructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_push_copy(cstr file, u32 line, const T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_copy_constructible_v<T>;
#endif

        bool _dyarr_insert(u64 index, T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_default_constructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_insert(cstr file, u32 line, u64 index, T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_default_constructible_v<T>;
#endif

        bool _dyarr_insert_ptr(u64 index, T value)
            requires std::is_pointer_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_insert_ptr(cstr file, u32 line, u64 index, T value)
            requires std::is_pointer_v<T>;
#endif

        bool _dyarr_insert_copy(u64 index, const T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_default_constructible_v<T> &&
                     std::is_copy_constructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_insert_copy(
            cstr file,
            u32 line,
            u64 index,
            const T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_default_constructible_v<T> &&
                     std::is_copy_constructible_v<T>;
#endif

        bool _dyarr_resize(u64 length)
            requires std::is_default_constructible_v<T> &&
                     mem::RelocatableObject<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _dyarr_resize(cstr file, u32 line, u64 length)
            requires std::is_default_constructible_v<T> &&
                     mem::RelocatableObject<T>;
#endif

        bool dyarr_reset()
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T> {
            return _reset_elements();
        }

        std::optional<T> dyarr_pop()
            requires std::is_move_constructible_v<T> &&
                     std::is_destructible_v<T>;
        std::optional<T> dyarr_remove(u64 index)
            requires mem::RelocatableObject<T>;

        T* data() noexcept { return m_data; }
        const T* data() const noexcept { return m_data; }
        u64 length() const noexcept { return m_length; }
        u64 capacity() const noexcept { return m_capacity; }
        bool empty() const noexcept { return m_length == 0; }
        mem::Allocator* allocator() noexcept { return m_allocator; }
        const mem::Allocator* allocator() const noexcept { return m_allocator; }
        bool owns_allocator() const noexcept {
            return m_allocator_destroy != nullptr;
        }

        T* begin() noexcept { return m_data; }
        const T* begin() const noexcept { return m_data; }
        const T* cbegin() const noexcept { return m_data; }

        T* end() noexcept {
            return m_data == nullptr ? nullptr : m_data + m_length;
        }
        const T* end() const noexcept {
            return m_data == nullptr ? nullptr : m_data + m_length;
        }
        const T* cend() const noexcept {
            return m_data == nullptr ? nullptr : m_data + m_length;
        }

    private:
        static void _diagnostic(cstr message) noexcept;
        [[noreturn]] static void _fatal(cstr message) noexcept;

        static T* _allocate(
            mem::Allocator& allocator,
            mem::SourceLocation source,
            u64 capacity) noexcept;
        static bool _free(
            mem::Allocator& allocator,
            mem::SourceLocation source,
            T* data,
            u64 capacity) noexcept;
        static u64 _initial_capacity(u64 requested_capacity) noexcept;
        static bool _next_capacity(
            u64 current_capacity,
            u64 minimum_capacity,
            u64& next_capacity) noexcept;

        bool _can_initialize_borrowed(mem::Allocator* allocator) const noexcept;
        bool _can_initialize_owned(const mem::AllocatorOwner& owner) const noexcept;
        bool _find_alias(const T* value, u64& index) const noexcept;

        bool _initialize_reserved(
            mem::SourceLocation source,
            mem::Allocator* allocator,
            u64 capacity)
            requires std::is_destructible_v<T>;
        bool _initialize_reserved_owned(
            mem::SourceLocation source,
            mem::AllocatorOwner&& allocator_owner,
            u64 capacity)
            requires std::is_destructible_v<T>;
        bool _initialize_length(
            mem::SourceLocation source,
            mem::Allocator* allocator,
            u64 capacity,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
        bool _initialize_length_owned(
            mem::SourceLocation source,
            mem::AllocatorOwner&& allocator_owner,
            u64 capacity,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
        bool _initialize_copy(
            mem::SourceLocation source,
            mem::Allocator* allocator,
            const T* values,
            u64 length)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
        bool _initialize_copy_owned(
            mem::SourceLocation source,
            mem::AllocatorOwner&& allocator_owner,
            const T* values,
            u64 length)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;

        bool _reserve(mem::SourceLocation source, u64 minimum_capacity)
            requires mem::RelocatableObject<T>;
        bool _resize_to(mem::SourceLocation source, u64 length)
            requires std::is_default_constructible_v<T> &&
                     mem::RelocatableObject<T>;
        bool _push_move(mem::SourceLocation source, T& value)
            requires mem::RelocatableObject<T>;
        bool _push_copy(mem::SourceLocation source, const T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_copy_constructible_v<T>;
        bool _insert_move(mem::SourceLocation source, u64 index, T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_default_constructible_v<T>;
        bool _insert_copy(
            mem::SourceLocation source,
            u64 index,
            const T& value)
            requires mem::RelocatableObject<T> &&
                     std::is_default_constructible_v<T> &&
                     std::is_copy_constructible_v<T>;

        bool _reset_elements()
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
        bool _release_storage(mem::SourceLocation source)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
        bool _shutdown(mem::SourceLocation source)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
        void _move_from(dyarr& other) noexcept;

        T* m_data = nullptr;
        u64 m_length = 0;
        u64 m_capacity = 0;
        mem::Allocator* m_allocator = nullptr;
        mem::AllocatorOwner::Destroy m_allocator_destroy = nullptr;

        friend class arr<T>;
    };

    template <IArrT T>
    dyarr<T>::dyarr(dyarr&& other) noexcept {
        _move_from(other);
    }

    template <IArrT T>
    dyarr<T>& dyarr<T>::operator=(dyarr&& other)
        noexcept(std::is_nothrow_destructible_v<T>) {
        if (this == &other)
            return *this;

        if (!_shutdown({__FILE__, __LINE__})) {
            _diagnostic("nk::cl::dyarr move assignment could not release its destination.");
            return *this;
        }

        _move_from(other);
        return *this;
    }

    template <IArrT T>
    dyarr<T>::~dyarr() noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        if (!_shutdown({__FILE__, __LINE__}))
            _diagnostic("nk::cl::dyarr destructor could not release its state.");
    }

    template <IArrT T>
    T& dyarr<T>::operator[](const u64 index) {
        Assert(index < m_length);
        return m_data[index];
    }

    template <IArrT T>
    const T& dyarr<T>::operator[](const u64 index) const {
        Assert(index < m_length);
        return m_data[index];
    }

    template <IArrT T>
    T& dyarr<T>::_dyarr_at(const u64 index)
        requires std::is_default_constructible_v<T> &&
                 mem::RelocatableObject<T> {
        if (index >= m_length) {
            if (index == numeric::u64_max ||
                !_resize_to({nullptr, 0}, index + 1)) {
                _fatal("nk::cl::dyarr_at could not grow the array.");
            }
        }
        return m_data[index];
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    T& dyarr<T>::_dyarr_at(
        cstr file,
        const u32 line,
        const u64 index)
        requires std::is_default_constructible_v<T> &&
                 mem::RelocatableObject<T> {
        if (index >= m_length) {
            if (index == numeric::u64_max ||
                !_resize_to({file, line}, index + 1)) {
                _fatal("nk::cl::dyarr_at could not grow the array.");
            }
        }
        return m_data[index];
    }
#endif

    template <IArrT T>
    const T& dyarr<T>::dyarr_at_const(const u64 index) const {
        Assert(index < m_length);
        return m_data[index];
    }

    template <IArrT T>
    bool dyarr<T>::_dyarr_init(
        mem::Allocator* allocator,
        const u64 capacity)
        requires std::is_destructible_v<T> {
        return _initialize_reserved({nullptr, 0}, allocator, capacity);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_init(
        cstr file,
        const u32 line,
        mem::Allocator* allocator,
        const u64 capacity)
        requires std::is_destructible_v<T> {
        return _initialize_reserved({file, line}, allocator, capacity);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_init_len(
        mem::Allocator* allocator,
        const u64 capacity,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_length(
            {nullptr, 0},
            allocator,
            capacity,
            length);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_init_len(
        cstr file,
        const u32 line,
        mem::Allocator* allocator,
        const u64 capacity,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_length(
            {file, line},
            allocator,
            capacity,
            length);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_init_own(
        mem::AllocatorOwner&& allocator_owner,
        const u64 capacity)
        requires std::is_destructible_v<T> {
        return _initialize_reserved_owned(
            {nullptr, 0},
            std::move(allocator_owner),
            capacity);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_init_own(
        cstr file,
        const u32 line,
        mem::AllocatorOwner&& allocator_owner,
        const u64 capacity)
        requires std::is_destructible_v<T> {
        return _initialize_reserved_owned(
            {file, line},
            std::move(allocator_owner),
            capacity);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_init_own_len(
        mem::AllocatorOwner&& allocator_owner,
        const u64 capacity,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_length_owned(
            {nullptr, 0},
            std::move(allocator_owner),
            capacity,
            length);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_init_own_len(
        cstr file,
        const u32 line,
        mem::AllocatorOwner&& allocator_owner,
        const u64 capacity,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_length_owned(
            {file, line},
            std::move(allocator_owner),
            capacity,
            length);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_init_list(
        mem::Allocator* allocator,
        const std::initializer_list<T> list)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_copy(
            {nullptr, 0},
            allocator,
            list.begin(),
            static_cast<u64>(list.size()));
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_init_list(
        cstr file,
        const u32 line,
        mem::Allocator* allocator,
        const std::initializer_list<T> list)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_copy(
            {file, line},
            allocator,
            list.begin(),
            static_cast<u64>(list.size()));
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_init_list_own(
        mem::AllocatorOwner&& allocator_owner,
        const std::initializer_list<T> list)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_copy_owned(
            {nullptr, 0},
            std::move(allocator_owner),
            list.begin(),
            static_cast<u64>(list.size()));
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_init_list_own(
        cstr file,
        const u32 line,
        mem::AllocatorOwner&& allocator_owner,
        const std::initializer_list<T> list)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_copy_owned(
            {file, line},
            std::move(allocator_owner),
            list.begin(),
            static_cast<u64>(list.size()));
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_clear()
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _release_storage({nullptr, 0});
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_clear(cstr file, const u32 line)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _release_storage({file, line});
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_shutdown()
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _shutdown({nullptr, 0});
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_shutdown(cstr file, const u32 line)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _shutdown({file, line});
    }
#endif

    template <IArrT T>
    T& dyarr<T>::dyarr_first() {
        Assert(m_length > 0, "nk::cl::dyarr::dyarr_first Array is empty!");
        return m_data[0];
    }

    template <IArrT T>
    const T& dyarr<T>::dyarr_first() const {
        Assert(m_length > 0, "nk::cl::dyarr::dyarr_first Array is empty!");
        return m_data[0];
    }

    template <IArrT T>
    T& dyarr<T>::dyarr_last() {
        Assert(m_length > 0, "nk::cl::dyarr::dyarr_last Array is empty!");
        return m_data[m_length - 1];
    }

    template <IArrT T>
    const T& dyarr<T>::dyarr_last() const {
        Assert(m_length > 0, "nk::cl::dyarr::dyarr_last Array is empty!");
        return m_data[m_length - 1];
    }

    template <IArrT T>
    bool dyarr<T>::_dyarr_push(T& value)
        requires mem::RelocatableObject<T> {
        return _push_move({nullptr, 0}, value);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_push(
        cstr file,
        const u32 line,
        T& value)
        requires mem::RelocatableObject<T> {
        return _push_move({file, line}, value);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_push_ptr(T value)
        requires std::is_pointer_v<T> {
        return _push_copy({nullptr, 0}, value);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_push_ptr(
        cstr file,
        const u32 line,
        T value)
        requires std::is_pointer_v<T> {
        return _push_copy({file, line}, value);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_push_copy(const T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_copy_constructible_v<T> {
        return _push_copy({nullptr, 0}, value);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_push_copy(
        cstr file,
        const u32 line,
        const T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_copy_constructible_v<T> {
        return _push_copy({file, line}, value);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_insert(const u64 index, T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_default_constructible_v<T> {
        return _insert_move({nullptr, 0}, index, value);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_insert(
        cstr file,
        const u32 line,
        const u64 index,
        T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_default_constructible_v<T> {
        return _insert_move({file, line}, index, value);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_insert_ptr(const u64 index, T value)
        requires std::is_pointer_v<T> {
        return _insert_copy({nullptr, 0}, index, value);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_insert_ptr(
        cstr file,
        const u32 line,
        const u64 index,
        T value)
        requires std::is_pointer_v<T> {
        return _insert_copy({file, line}, index, value);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_insert_copy(
        const u64 index,
        const T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_default_constructible_v<T> &&
                 std::is_copy_constructible_v<T> {
        return _insert_copy({nullptr, 0}, index, value);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_insert_copy(
        cstr file,
        const u32 line,
        const u64 index,
        const T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_default_constructible_v<T> &&
                 std::is_copy_constructible_v<T> {
        return _insert_copy({file, line}, index, value);
    }
#endif

    template <IArrT T>
    bool dyarr<T>::_dyarr_resize(const u64 length)
        requires std::is_default_constructible_v<T> &&
                 mem::RelocatableObject<T> {
        return _resize_to({nullptr, 0}, length);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool dyarr<T>::_dyarr_resize(
        cstr file,
        const u32 line,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 mem::RelocatableObject<T> {
        return _resize_to({file, line}, length);
    }
#endif

    template <IArrT T>
    std::optional<T> dyarr<T>::dyarr_pop()
        requires std::is_move_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (m_length == 0)
            return std::nullopt;

        const u64 index = m_length - 1;
        std::optional<T> result{
            std::in_place,
            std::move(m_data[index])};
        mem::destroy_range(m_data + index, 1);
        m_length = index;
        return result;
    }

    template <IArrT T>
    std::optional<T> dyarr<T>::dyarr_remove(const u64 index)
        requires mem::RelocatableObject<T> {
        if (index >= m_length) {
            _diagnostic("nk::cl::dyarr_remove index out of bounds.");
            return std::nullopt;
        }

        std::optional<T> result{
            std::in_place,
            std::move(m_data[index])};
        mem::destroy_range(m_data + index, 1);
        mem::relocate_range(
            m_data + index,
            m_data + index + 1,
            m_length - index - 1);
        --m_length;
        return result;
    }

    template <IArrT T>
    void dyarr<T>::_diagnostic(const cstr message) noexcept {
        os::write(message, std::strlen(message));
        os::write("\n", 1);
        os::flush();
    }

    template <IArrT T>
    [[noreturn]] void dyarr<T>::_fatal(const cstr message) noexcept {
        _diagnostic(message);
        std::abort();
    }

    template <IArrT T>
    T* dyarr<T>::_allocate(
        mem::Allocator& allocator,
        const mem::SourceLocation source,
        const u64 capacity) noexcept {
        if (capacity == 0)
            return nullptr;

#if NK_MEMORY_TRACKING_ENABLED
        return allocator._allocate_lot_t<T>(
            source.file,
            source.line,
            capacity);
#else
        static_cast<void>(source);
        return allocator._allocate_lot_t<T>(capacity);
#endif
    }

    template <IArrT T>
    bool dyarr<T>::_free(
        mem::Allocator& allocator,
        const mem::SourceLocation source,
        T* data,
        const u64 capacity) noexcept {
#if NK_MEMORY_TRACKING_ENABLED
        return allocator._free_lot_t<T>(
            source.file,
            source.line,
            data,
            capacity);
#else
        static_cast<void>(source);
        return allocator._free_lot_t<T>(data, capacity);
#endif
    }

    template <IArrT T>
    u64 dyarr<T>::_initial_capacity(const u64 requested_capacity) noexcept {
        return requested_capacity < 4 ? 4 : requested_capacity;
    }

    template <IArrT T>
    bool dyarr<T>::_next_capacity(
        const u64 current_capacity,
        const u64 minimum_capacity,
        u64& next_capacity) noexcept {
        if (minimum_capacity <= current_capacity) {
            next_capacity = current_capacity;
            return true;
        }

        const u64 doubled_capacity =
            current_capacity > numeric::u64_max / 2
                ? numeric::u64_max
                : current_capacity * 2;
        next_capacity = _initial_capacity(doubled_capacity);
        if (next_capacity < minimum_capacity)
            next_capacity = minimum_capacity;
        return next_capacity >= minimum_capacity;
    }

    template <IArrT T>
    bool dyarr<T>::_can_initialize_borrowed(
        mem::Allocator* allocator) const noexcept {
        if (allocator == nullptr || m_data != nullptr ||
            m_length != 0 || m_capacity != 0) {
            return false;
        }

        if (m_allocator_destroy != nullptr && m_allocator != allocator)
            return false;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_can_initialize_owned(
        const mem::AllocatorOwner& owner) const noexcept {
        return owner && m_data == nullptr && m_length == 0 &&
               m_capacity == 0 && m_allocator == nullptr &&
               m_allocator_destroy == nullptr;
    }

    template <IArrT T>
    bool dyarr<T>::_find_alias(const T* value, u64& index) const noexcept {
        if (value == nullptr || m_data == nullptr || m_length == 0)
            return false;

        const auto begin_address =
            reinterpret_cast<std::uintptr_t>(m_data);
        const auto value_address =
            reinterpret_cast<std::uintptr_t>(value);
        if (value_address < begin_address)
            return false;

        const auto offset = value_address - begin_address;
        if (offset % sizeof(T) != 0)
            return false;

        const auto candidate = static_cast<u64>(offset / sizeof(T));
        if (candidate >= m_length)
            return false;

        index = candidate;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_initialize_reserved(
        const mem::SourceLocation source,
        mem::Allocator* allocator,
        const u64 capacity)
        requires std::is_destructible_v<T> {
        if (!_can_initialize_borrowed(allocator))
            return false;

        const u64 actual_capacity = _initial_capacity(capacity);
        T* data = _allocate(*allocator, source, actual_capacity);
        if (data == nullptr)
            return false;

        m_data = data;
        m_capacity = actual_capacity;
        m_allocator = allocator;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_initialize_reserved_owned(
        const mem::SourceLocation source,
        mem::AllocatorOwner&& allocator_owner,
        const u64 capacity)
        requires std::is_destructible_v<T> {
        if (!_can_initialize_owned(allocator_owner))
            return false;

        mem::Allocator* allocator = allocator_owner.get();
        const u64 actual_capacity = _initial_capacity(capacity);
        T* data = _allocate(*allocator, source, actual_capacity);
        if (data == nullptr)
            return false;

        const mem::AllocatorOwner::Ownership ownership =
            allocator_owner.release();
        m_data = data;
        m_capacity = actual_capacity;
        m_allocator = ownership.allocator;
        m_allocator_destroy = ownership.destroy;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_initialize_length(
        const mem::SourceLocation source,
        mem::Allocator* allocator,
        const u64 capacity,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_can_initialize_borrowed(allocator))
            return false;

        const u64 requested_capacity =
            capacity < length ? length : capacity;
        const u64 actual_capacity = _initial_capacity(requested_capacity);
        T* data = _allocate(*allocator, source, actual_capacity);
        if (data == nullptr)
            return false;

        mem::construct_value_range(data, length);
        m_data = data;
        m_length = length;
        m_capacity = actual_capacity;
        m_allocator = allocator;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_initialize_length_owned(
        const mem::SourceLocation source,
        mem::AllocatorOwner&& allocator_owner,
        const u64 capacity,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_can_initialize_owned(allocator_owner))
            return false;

        mem::Allocator* allocator = allocator_owner.get();
        const u64 requested_capacity =
            capacity < length ? length : capacity;
        const u64 actual_capacity = _initial_capacity(requested_capacity);
        T* data = _allocate(*allocator, source, actual_capacity);
        if (data == nullptr)
            return false;

        mem::construct_value_range(data, length);
        const mem::AllocatorOwner::Ownership ownership =
            allocator_owner.release();
        m_data = data;
        m_length = length;
        m_capacity = actual_capacity;
        m_allocator = ownership.allocator;
        m_allocator_destroy = ownership.destroy;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_initialize_copy(
        const mem::SourceLocation source,
        mem::Allocator* allocator,
        const T* values,
        const u64 length)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_can_initialize_borrowed(allocator))
            return false;

        const u64 actual_capacity = _initial_capacity(length);
        T* data = _allocate(*allocator, source, actual_capacity);
        if (data == nullptr)
            return false;

        mem::construct_copy_range(data, values, length);
        m_data = data;
        m_length = length;
        m_capacity = actual_capacity;
        m_allocator = allocator;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_initialize_copy_owned(
        const mem::SourceLocation source,
        mem::AllocatorOwner&& allocator_owner,
        const T* values,
        const u64 length)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_can_initialize_owned(allocator_owner))
            return false;

        mem::Allocator* allocator = allocator_owner.get();
        const u64 actual_capacity = _initial_capacity(length);
        T* data = _allocate(*allocator, source, actual_capacity);
        if (data == nullptr)
            return false;

        mem::construct_copy_range(data, values, length);
        const mem::AllocatorOwner::Ownership ownership =
            allocator_owner.release();
        m_data = data;
        m_length = length;
        m_capacity = actual_capacity;
        m_allocator = ownership.allocator;
        m_allocator_destroy = ownership.destroy;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_reserve(
        const mem::SourceLocation source,
        const u64 minimum_capacity)
        requires mem::RelocatableObject<T> {
        if (minimum_capacity <= m_capacity)
            return true;
        if (m_allocator == nullptr)
            return false;

        u64 next_capacity = 0;
        if (!_next_capacity(m_capacity, minimum_capacity, next_capacity))
            return false;

        T* data = _allocate(*m_allocator, source, next_capacity);
        if (data == nullptr)
            return false;

        mem::relocate_range(data, m_data, m_length);
        const bool released_previous_storage =
            m_data == nullptr ||
            _free(*m_allocator, source, m_data, m_capacity);

        m_data = data;
        m_capacity = next_capacity;

        if (!released_previous_storage) {
            _diagnostic(
                "nk::cl::dyarr could not release its previous storage after growth.");
        }
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_resize_to(
        const mem::SourceLocation source,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 mem::RelocatableObject<T> {
        if (length == m_length)
            return true;

        if (length < m_length) {
            mem::destroy_range(m_data + length, m_length - length);
            m_length = length;
            return true;
        }

        if (!_reserve(source, length))
            return false;

        mem::construct_value_range(m_data + m_length, length - m_length);
        m_length = length;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_push_move(
        const mem::SourceLocation source,
        T& value)
        requires mem::RelocatableObject<T> {
        if (m_length < m_capacity) {
            (void)mem::construct_object(
                m_data + m_length,
                std::move(value));
            ++m_length;
            return true;
        }

        if (m_length == numeric::u64_max)
            return false;

        u64 alias_index = 0;
        const bool aliases_storage = _find_alias(&value, alias_index);
        if (!_reserve(source, m_length + 1))
            return false;

        T& current_value =
            aliases_storage ? m_data[alias_index] : value;
        (void)mem::construct_object(
            m_data + m_length,
            std::move(current_value));
        ++m_length;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_push_copy(
        const mem::SourceLocation source,
        const T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_copy_constructible_v<T> {
        if (m_length < m_capacity) {
            (void)mem::construct_object(m_data + m_length, value);
            ++m_length;
            return true;
        }

        if (m_length == numeric::u64_max)
            return false;

        u64 alias_index = 0;
        const bool aliases_storage = _find_alias(&value, alias_index);
        if (!_reserve(source, m_length + 1))
            return false;

        const T& current_value =
            aliases_storage ? m_data[alias_index] : value;
        (void)mem::construct_object(m_data + m_length, current_value);
        ++m_length;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_insert_move(
        const mem::SourceLocation source,
        const u64 index,
        T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_default_constructible_v<T> {
        const bool appends_beyond_end = index >= m_length;
        if (index == numeric::u64_max ||
            (!appends_beyond_end && m_length == numeric::u64_max)) {
            return false;
        }

        const u64 required_length =
            appends_beyond_end ? index + 1 : m_length + 1;
        const bool requires_growth = required_length > m_capacity;
        u64 alias_index = 0;
        const bool aliases_storage =
            requires_growth && _find_alias(&value, alias_index);
        if (!_reserve(source, required_length))
            return false;

        T& current_value =
            aliases_storage ? m_data[alias_index] : value;
        if (appends_beyond_end) {
            mem::construct_value_range(
                m_data + m_length,
                index - m_length);
            (void)mem::construct_object(
                m_data + index,
                std::move(current_value));
            m_length = required_length;
            return true;
        }

        T temporary{std::move(current_value)};
        mem::relocate_range(
            m_data + index + 1,
            m_data + index,
            m_length - index);
        (void)mem::construct_object(m_data + index, std::move(temporary));
        ++m_length;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_insert_copy(
        const mem::SourceLocation source,
        const u64 index,
        const T& value)
        requires mem::RelocatableObject<T> &&
                 std::is_default_constructible_v<T> &&
                 std::is_copy_constructible_v<T> {
        const bool appends_beyond_end = index >= m_length;
        if (index == numeric::u64_max ||
            (!appends_beyond_end && m_length == numeric::u64_max)) {
            return false;
        }

        const u64 required_length =
            appends_beyond_end ? index + 1 : m_length + 1;
        const bool requires_growth = required_length > m_capacity;
        u64 alias_index = 0;
        const bool aliases_storage =
            requires_growth && _find_alias(&value, alias_index);
        if (!_reserve(source, required_length))
            return false;

        const T& current_value =
            aliases_storage ? m_data[alias_index] : value;
        if (appends_beyond_end) {
            mem::construct_value_range(
                m_data + m_length,
                index - m_length);
            (void)mem::construct_object(m_data + index, current_value);
            m_length = required_length;
            return true;
        }

        T temporary{current_value};
        mem::relocate_range(
            m_data + index + 1,
            m_data + index,
            m_length - index);
        (void)mem::construct_object(m_data + index, std::move(temporary));
        ++m_length;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_reset_elements()
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        if (m_data == nullptr)
            return m_length == 0 && m_capacity == 0;

        mem::destroy_range(m_data, m_length);
        m_length = 0;
        return true;
    }

    template <IArrT T>
    bool dyarr<T>::_release_storage(
        const mem::SourceLocation source)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        if (m_data == nullptr)
            return m_length == 0 && m_capacity == 0;
        if (m_allocator == nullptr)
            return false;

        T* data = m_data;
        const u64 length = m_length;
        const u64 capacity = m_capacity;
        mem::destroy_range(data, length);
        const bool freed = _free(*m_allocator, source, data, capacity);

        m_data = nullptr;
        m_length = 0;
        m_capacity = 0;
        return freed;
    }

    template <IArrT T>
    bool dyarr<T>::_shutdown(const mem::SourceLocation source)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        if (!_release_storage(source))
            return false;

        mem::Allocator* allocator = m_allocator;
        mem::AllocatorOwner::Destroy destroy = m_allocator_destroy;
        m_allocator = nullptr;
        m_allocator_destroy = nullptr;
        if (destroy != nullptr)
            destroy(allocator);
        return true;
    }

    template <IArrT T>
    void dyarr<T>::_move_from(dyarr& other) noexcept {
        m_data = other.m_data;
        m_length = other.m_length;
        m_capacity = other.m_capacity;
        m_allocator = other.m_allocator;
        m_allocator_destroy = other.m_allocator_destroy;

        other.m_data = nullptr;
        other.m_length = 0;
        other.m_capacity = 0;
        other.m_allocator = nullptr;
        other.m_allocator_destroy = nullptr;
    }
}

#if NK_MEMORY_TRACKING_ENABLED

    #define dyarr_at(index) \
        _dyarr_at(__FILE__, __LINE__, (index))

    #define dyarr_init(allocator, capacity) \
        _dyarr_init(__FILE__, __LINE__, (allocator), (capacity))

    #define dyarr_init_len(allocator, capacity, length) \
        _dyarr_init_len(__FILE__, __LINE__, (allocator), (capacity), (length))

    #define dyarr_init_own(allocator_owner, capacity) \
        _dyarr_init_own(__FILE__, __LINE__, (allocator_owner), (capacity))

    #define dyarr_init_own_len(allocator_owner, capacity, length) \
        _dyarr_init_own_len( \
            __FILE__, \
            __LINE__, \
            (allocator_owner), \
            (capacity), \
            (length))

    #define dyarr_init_list(allocator, ...) \
        _dyarr_init_list(__FILE__, __LINE__, (allocator), __VA_ARGS__)

    #define dyarr_init_list_own(allocator_owner, ...) \
        _dyarr_init_list_own( \
            __FILE__, \
            __LINE__, \
            (allocator_owner), \
            __VA_ARGS__)

    #define dyarr_clear() \
        _dyarr_clear(__FILE__, __LINE__)

    #define dyarr_shutdown() \
        _dyarr_shutdown(__FILE__, __LINE__)

    #define dyarr_push(value) \
        _dyarr_push(__FILE__, __LINE__, (value))

    #define dyarr_push_ptr(value) \
        _dyarr_push_ptr(__FILE__, __LINE__, (value))

    #define dyarr_push_copy(...) \
        _dyarr_push_copy(__FILE__, __LINE__, (__VA_ARGS__))

    #define dyarr_insert(index, value) \
        _dyarr_insert(__FILE__, __LINE__, (index), (value))

    #define dyarr_insert_ptr(index, value) \
        _dyarr_insert_ptr(__FILE__, __LINE__, (index), (value))

    #define dyarr_insert_copy(index, ...) \
        _dyarr_insert_copy(__FILE__, __LINE__, (index), (__VA_ARGS__))

    #define dyarr_resize(length) \
        _dyarr_resize(__FILE__, __LINE__, (length))

#else

    #define dyarr_at(index) \
        _dyarr_at((index))

    #define dyarr_init(allocator, capacity) \
        _dyarr_init((allocator), (capacity))

    #define dyarr_init_len(allocator, capacity, length) \
        _dyarr_init_len((allocator), (capacity), (length))

    #define dyarr_init_own(allocator_owner, capacity) \
        _dyarr_init_own((allocator_owner), (capacity))

    #define dyarr_init_own_len(allocator_owner, capacity, length) \
        _dyarr_init_own_len((allocator_owner), (capacity), (length))

    #define dyarr_init_list(allocator, ...) \
        _dyarr_init_list((allocator), __VA_ARGS__)

    #define dyarr_init_list_own(allocator_owner, ...) \
        _dyarr_init_list_own((allocator_owner), __VA_ARGS__)

    #define dyarr_clear() \
        _dyarr_clear()

    #define dyarr_shutdown() \
        _dyarr_shutdown()

    #define dyarr_push(value) \
        _dyarr_push((value))

    #define dyarr_push_ptr(value) \
        _dyarr_push_ptr((value))

    #define dyarr_push_copy(...) \
        _dyarr_push_copy((__VA_ARGS__))

    #define dyarr_insert(index, value) \
        _dyarr_insert((index), (value))

    #define dyarr_insert_ptr(index, value) \
        _dyarr_insert_ptr((index), (value))

    #define dyarr_insert_copy(index, ...) \
        _dyarr_insert_copy((index), (__VA_ARGS__))

    #define dyarr_resize(length) \
        _dyarr_resize((length))

#endif
