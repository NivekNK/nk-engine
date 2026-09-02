#pragma once

#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "collections/arr_type.h"
#include "memory/allocator_owner.h"

namespace nk::cl {
    template <IArrT>
    class dyarr;

    template <IArrT T>
    class arr {
    public:
        arr() noexcept = default;

        arr(arr&& other) noexcept;
        arr& operator=(arr&& other)
            noexcept(std::is_nothrow_destructible_v<T>);

        arr(dyarr<T>& other)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
        arr(dyarr<T>&& other)
            requires mem::RelocatableObject<T>;

        arr(const arr&) = delete;
        arr& operator=(const arr&) = delete;

        ~arr() noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;

        T& operator[](u64 index);
        const T& operator[](u64 index) const;

        T& at(u64 index);
        const T& at(u64 index) const;

        bool _arr_init(mem::Allocator* allocator, u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_init(
            cstr file,
            u32 line,
            mem::Allocator* allocator,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
#endif

        bool _arr_init_own(mem::AllocatorOwner&& allocator_owner, u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_init_own(
            cstr file,
            u32 line,
            mem::AllocatorOwner&& allocator_owner,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
#endif

        // An erased Allocator* cannot carry a correctly typed destructor.
        // These transitional overloads compile but reject ownership safely.
        bool _arr_init_own(mem::Allocator* allocator, u64 length);
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_init_own(
            cstr file,
            u32 line,
            mem::Allocator* allocator,
            u64 length);
#endif

        bool _arr_init_list(
            mem::Allocator* allocator,
            std::initializer_list<T> list)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_init_list(
            cstr file,
            u32 line,
            mem::Allocator* allocator,
            std::initializer_list<T> list)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
#endif

        bool _arr_init_list_own(
            mem::AllocatorOwner&& allocator_owner,
            std::initializer_list<T> list)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_init_list_own(
            cstr file,
            u32 line,
            mem::AllocatorOwner&& allocator_owner,
            std::initializer_list<T> list)
            requires std::is_copy_constructible_v<T> &&
                     std::is_destructible_v<T>;
#endif

        bool _arr_init_list_own(
            mem::Allocator* allocator,
            std::initializer_list<T> list);
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_init_list_own(
            cstr file,
            u32 line,
            mem::Allocator* allocator,
            std::initializer_list<T> list);
#endif

        bool _arr_clear()
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_clear(cstr file, u32 line)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#endif
        bool _arr_clear(mem::Allocator* allocator)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_clear(cstr file, u32 line, mem::Allocator* allocator)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#endif

        bool _arr_shutdown()
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_shutdown(cstr file, u32 line)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#endif
        bool _arr_shutdown(mem::Allocator* allocator)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#if NK_MEMORY_TRACKING_ENABLED
        bool _arr_shutdown(cstr file, u32 line, mem::Allocator* allocator)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
#endif

        T& first();
        const T& first() const;

        T& last();
        const T& last() const;

        bool arr_reset()
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T> {
            return _shutdown({nullptr, 0}, nullptr);
        }

        T* data() noexcept { return m_data; }
        const T* data() const noexcept { return m_data; }
        u64 length() const noexcept { return m_length; }
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

        static T* _allocate(
            mem::Allocator& allocator,
            mem::SourceLocation source,
            u64 length) noexcept;
        static bool _free(
            mem::Allocator& allocator,
            mem::SourceLocation source,
            T* data,
            u64 length) noexcept;

        bool _can_initialize_borrowed(mem::Allocator* allocator) const noexcept;
        bool _can_initialize_owned(const mem::AllocatorOwner& owner) const noexcept;

        bool _initialize_value(
            mem::SourceLocation source,
            mem::Allocator* allocator,
            u64 length)
            requires std::is_default_constructible_v<T> &&
                     std::is_destructible_v<T>;
        bool _initialize_value_owned(
            mem::SourceLocation source,
            mem::AllocatorOwner&& allocator_owner,
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

        bool _release_storage(
            mem::SourceLocation source,
            mem::Allocator* fallback_allocator)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
        bool _shutdown(
            mem::SourceLocation source,
            mem::Allocator* fallback_allocator)
            noexcept(std::is_nothrow_destructible_v<T>)
            requires std::is_destructible_v<T>;
        void _move_from(arr& other) noexcept;

        T* m_data = nullptr;
        u64 m_length = 0;
        mem::Allocator* m_allocator = nullptr;
        mem::AllocatorOwner::Destroy m_allocator_destroy = nullptr;

        friend class dyarr<T>;
    };

    template <IArrT T>
    arr<T>::arr(arr&& other) noexcept {
        _move_from(other);
    }

    template <IArrT T>
    arr<T>& arr<T>::operator=(arr&& other)
        noexcept(std::is_nothrow_destructible_v<T>) {
        if (this == &other)
            return *this;

        if (!_shutdown({__FILE__, __LINE__}, nullptr)) {
            _diagnostic("nk::cl::arr move assignment could not release its destination.");
            return *this;
        }

        _move_from(other);
        return *this;
    }

    template <IArrT T>
    arr<T>::arr(dyarr<T>& other)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_initialize_copy(
                {__FILE__, __LINE__},
                other.m_allocator,
                other.m_data,
                other.m_length)) {
            _diagnostic("nk::cl::arr could not copy from dyarr.");
        }
    }

    template <IArrT T>
    arr<T>::arr(dyarr<T>&& other)
        requires mem::RelocatableObject<T> {
        if (other.m_own_allocator) {
            _diagnostic("nk::cl::arr cannot transfer legacy erased allocator ownership.");
            return;
        }

        if (other.m_capacity == other.m_length) {
            m_data = other.m_data;
            m_length = other.m_length;
            m_allocator = other.m_allocator;

            other.m_data = nullptr;
            other.m_length = 0;
            other.m_capacity = 0;
            other.m_allocator = nullptr;
            return;
        }

        if (other.m_length == 0) {
            if (other.m_capacity > 0 &&
                !_free(
                    *other.m_allocator,
                    {__FILE__, __LINE__},
                    other.m_data,
                    other.m_capacity)) {
                _diagnostic("nk::cl::arr could not release empty dyarr storage.");
                return;
            }

            m_allocator = other.m_allocator;
            other.m_data = nullptr;
            other.m_capacity = 0;
            other.m_allocator = nullptr;
            return;
        }

        T* destination = _allocate(
            *other.m_allocator,
            {__FILE__, __LINE__},
            other.m_length);
        if (destination == nullptr) {
            _diagnostic("nk::cl::arr could not allocate while moving from dyarr.");
            return;
        }

        mem::relocate_range(destination, other.m_data, other.m_length);
        const bool source_storage_freed = _free(
            *other.m_allocator,
            {__FILE__, __LINE__},
            other.m_data,
            other.m_capacity);

        m_data = destination;
        m_length = other.m_length;
        m_allocator = other.m_allocator;

        other.m_data = nullptr;
        other.m_length = 0;
        other.m_capacity = 0;
        other.m_allocator = nullptr;

        if (!source_storage_freed)
            _diagnostic("nk::cl::arr could not release dyarr storage after relocation.");
    }

    template <IArrT T>
    arr<T>::~arr() noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        if (!_shutdown({__FILE__, __LINE__}, nullptr))
            _diagnostic("nk::cl::arr destructor could not release its state.");
    }

    template <IArrT T>
    T& arr<T>::operator[](const u64 index) {
        Assert(index < m_length);
        return m_data[index];
    }

    template <IArrT T>
    const T& arr<T>::operator[](const u64 index) const {
        Assert(index < m_length);
        return m_data[index];
    }

    template <IArrT T>
    T& arr<T>::at(const u64 index) {
        Assert(index < m_length);
        return m_data[index];
    }

    template <IArrT T>
    const T& arr<T>::at(const u64 index) const {
        Assert(index < m_length);
        return m_data[index];
    }

    template <IArrT T>
    bool arr<T>::_arr_init(mem::Allocator* allocator, const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_value({nullptr, 0}, allocator, length);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool arr<T>::_arr_init(
        cstr file,
        const u32 line,
        mem::Allocator* allocator,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_value({file, line}, allocator, length);
    }
#endif

    template <IArrT T>
    bool arr<T>::_arr_init_own(
        mem::AllocatorOwner&& allocator_owner,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_value_owned(
            {nullptr, 0},
            std::move(allocator_owner),
            length);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool arr<T>::_arr_init_own(
        cstr file,
        const u32 line,
        mem::AllocatorOwner&& allocator_owner,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        return _initialize_value_owned(
            {file, line},
            std::move(allocator_owner),
            length);
    }
#endif

    template <IArrT T>
    bool arr<T>::_arr_init_own(mem::Allocator*, u64) {
        _diagnostic("nk::cl::arr ownership requires mem::AllocatorOwner.");
        return false;
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool arr<T>::_arr_init_own(cstr, u32, mem::Allocator*, u64) {
        _diagnostic("nk::cl::arr ownership requires mem::AllocatorOwner.");
        return false;
    }
#endif

    template <IArrT T>
    bool arr<T>::_arr_init_list(
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
    bool arr<T>::_arr_init_list(
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
    bool arr<T>::_arr_init_list_own(
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
    bool arr<T>::_arr_init_list_own(
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
    bool arr<T>::_arr_init_list_own(
        mem::Allocator*,
        std::initializer_list<T>) {
        _diagnostic("nk::cl::arr ownership requires mem::AllocatorOwner.");
        return false;
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool arr<T>::_arr_init_list_own(
        cstr,
        u32,
        mem::Allocator*,
        std::initializer_list<T>) {
        _diagnostic("nk::cl::arr ownership requires mem::AllocatorOwner.");
        return false;
    }
#endif

    template <IArrT T>
    bool arr<T>::_arr_clear()
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _release_storage({nullptr, 0}, nullptr);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool arr<T>::_arr_clear(cstr file, const u32 line)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _release_storage({file, line}, nullptr);
    }
#endif

    template <IArrT T>
    bool arr<T>::_arr_clear(mem::Allocator* allocator)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _release_storage({nullptr, 0}, allocator);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool arr<T>::_arr_clear(
        cstr file,
        const u32 line,
        mem::Allocator* allocator)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _release_storage({file, line}, allocator);
    }
#endif

    template <IArrT T>
    bool arr<T>::_arr_shutdown()
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _shutdown({nullptr, 0}, nullptr);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool arr<T>::_arr_shutdown(cstr file, const u32 line)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _shutdown({file, line}, nullptr);
    }
#endif

    template <IArrT T>
    bool arr<T>::_arr_shutdown(mem::Allocator* allocator)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _shutdown({nullptr, 0}, allocator);
    }

#if NK_MEMORY_TRACKING_ENABLED
    template <IArrT T>
    bool arr<T>::_arr_shutdown(
        cstr file,
        const u32 line,
        mem::Allocator* allocator)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        return _shutdown({file, line}, allocator);
    }
#endif

    template <IArrT T>
    T& arr<T>::first() {
        Assert(m_length > 0);
        return m_data[0];
    }

    template <IArrT T>
    const T& arr<T>::first() const {
        Assert(m_length > 0);
        return m_data[0];
    }

    template <IArrT T>
    T& arr<T>::last() {
        Assert(m_length > 0);
        return m_data[m_length - 1];
    }

    template <IArrT T>
    const T& arr<T>::last() const {
        Assert(m_length > 0);
        return m_data[m_length - 1];
    }

    template <IArrT T>
    void arr<T>::_diagnostic(const cstr message) noexcept {
        os::write(message, std::strlen(message));
        os::write("\n", 1);
        os::flush();
    }

    template <IArrT T>
    T* arr<T>::_allocate(
        mem::Allocator& allocator,
        const mem::SourceLocation source,
        const u64 length) noexcept {
        if (length == 0)
            return nullptr;

#if NK_MEMORY_TRACKING_ENABLED
        return allocator._allocate_lot_t<T>(source.file, source.line, length);
#else
        static_cast<void>(source);
        return allocator._allocate_lot_t<T>(length);
#endif
    }

    template <IArrT T>
    bool arr<T>::_free(
        mem::Allocator& allocator,
        const mem::SourceLocation source,
        T* data,
        const u64 length) noexcept {
#if NK_MEMORY_TRACKING_ENABLED
        return allocator._free_lot_t<T>(source.file, source.line, data, length);
#else
        static_cast<void>(source);
        return allocator._free_lot_t<T>(data, length);
#endif
    }

    template <IArrT T>
    bool arr<T>::_can_initialize_borrowed(mem::Allocator* allocator) const noexcept {
        if (allocator == nullptr || m_data != nullptr || m_length != 0)
            return false;

        if (m_allocator_destroy != nullptr && m_allocator != allocator)
            return false;

        return true;
    }

    template <IArrT T>
    bool arr<T>::_can_initialize_owned(
        const mem::AllocatorOwner& owner) const noexcept {
        return owner && m_data == nullptr && m_length == 0 &&
               m_allocator == nullptr && m_allocator_destroy == nullptr;
    }

    template <IArrT T>
    bool arr<T>::_initialize_value(
        const mem::SourceLocation source,
        mem::Allocator* allocator,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_can_initialize_borrowed(allocator))
            return false;

        T* data = _allocate(*allocator, source, length);
        if (length > 0 && data == nullptr)
            return false;

        mem::construct_value_range(data, length);
        m_data = data;
        m_length = length;
        m_allocator = allocator;
        return true;
    }

    template <IArrT T>
    bool arr<T>::_initialize_value_owned(
        const mem::SourceLocation source,
        mem::AllocatorOwner&& allocator_owner,
        const u64 length)
        requires std::is_default_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_can_initialize_owned(allocator_owner))
            return false;

        mem::Allocator* allocator = allocator_owner.get();
        T* data = _allocate(*allocator, source, length);
        if (length > 0 && data == nullptr)
            return false;

        mem::construct_value_range(data, length);
        const mem::AllocatorOwner::Ownership ownership =
            allocator_owner.release();
        m_data = data;
        m_length = length;
        m_allocator = ownership.allocator;
        m_allocator_destroy = ownership.destroy;
        return true;
    }

    template <IArrT T>
    bool arr<T>::_initialize_copy(
        const mem::SourceLocation source,
        mem::Allocator* allocator,
        const T* values,
        const u64 length)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_can_initialize_borrowed(allocator))
            return false;

        T* data = _allocate(*allocator, source, length);
        if (length > 0 && data == nullptr)
            return false;

        mem::construct_copy_range(data, values, length);
        m_data = data;
        m_length = length;
        m_allocator = allocator;
        return true;
    }

    template <IArrT T>
    bool arr<T>::_initialize_copy_owned(
        const mem::SourceLocation source,
        mem::AllocatorOwner&& allocator_owner,
        const T* values,
        const u64 length)
        requires std::is_copy_constructible_v<T> &&
                 std::is_destructible_v<T> {
        if (!_can_initialize_owned(allocator_owner))
            return false;

        mem::Allocator* allocator = allocator_owner.get();
        T* data = _allocate(*allocator, source, length);
        if (length > 0 && data == nullptr)
            return false;

        mem::construct_copy_range(data, values, length);
        const mem::AllocatorOwner::Ownership ownership =
            allocator_owner.release();
        m_data = data;
        m_length = length;
        m_allocator = ownership.allocator;
        m_allocator_destroy = ownership.destroy;
        return true;
    }

    template <IArrT T>
    bool arr<T>::_release_storage(
        const mem::SourceLocation source,
        mem::Allocator* fallback_allocator)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        if (m_data == nullptr)
            return m_length == 0;

        mem::Allocator* allocator =
            m_allocator != nullptr ? m_allocator : fallback_allocator;
        if (allocator == nullptr)
            return false;

        T* data = m_data;
        const u64 length = m_length;
        mem::destroy_range(data, length);
        const bool freed = _free(*allocator, source, data, length);

        m_data = nullptr;
        m_length = 0;
        return freed;
    }

    template <IArrT T>
    bool arr<T>::_shutdown(
        const mem::SourceLocation source,
        mem::Allocator* fallback_allocator)
        noexcept(std::is_nothrow_destructible_v<T>)
        requires std::is_destructible_v<T> {
        if (!_release_storage(source, fallback_allocator))
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
    void arr<T>::_move_from(arr& other) noexcept {
        m_data = other.m_data;
        m_length = other.m_length;
        m_allocator = other.m_allocator;
        m_allocator_destroy = other.m_allocator_destroy;

        other.m_data = nullptr;
        other.m_length = 0;
        other.m_allocator = nullptr;
        other.m_allocator_destroy = nullptr;
    }
}

#if NK_MEMORY_TRACKING_ENABLED

    #define arr_init(allocator, length) \
        _arr_init(__FILE__, __LINE__, (allocator), (length))

    #define arr_init_own(allocator_owner, length) \
        _arr_init_own(__FILE__, __LINE__, (allocator_owner), (length))

    #define arr_init_list(allocator, ...) \
        _arr_init_list(__FILE__, __LINE__, (allocator), __VA_ARGS__)

    #define arr_init_list_own(allocator_owner, ...) \
        _arr_init_list_own(__FILE__, __LINE__, (allocator_owner), __VA_ARGS__)

    #define arr_clear() \
        _arr_clear(__FILE__, __LINE__)

    #define arr_clear_allocator(allocator) \
        _arr_clear(__FILE__, __LINE__, (allocator))

    #define arr_shutdown() \
        _arr_shutdown(__FILE__, __LINE__)

    #define arr_shutdown_allocator(allocator) \
        _arr_shutdown(__FILE__, __LINE__, (allocator))

#else

    #define arr_init(allocator, length) \
        _arr_init((allocator), (length))

    #define arr_init_own(allocator_owner, length) \
        _arr_init_own((allocator_owner), (length))

    #define arr_init_list(allocator, ...) \
        _arr_init_list((allocator), __VA_ARGS__)

    #define arr_init_list_own(allocator_owner, ...) \
        _arr_init_list_own((allocator_owner), __VA_ARGS__)

    #define arr_clear() \
        _arr_clear()

    #define arr_clear_allocator(allocator) \
        _arr_clear((allocator))

    #define arr_shutdown() \
        _arr_shutdown()

    #define arr_shutdown_allocator(allocator) \
        _arr_shutdown((allocator))

#endif
