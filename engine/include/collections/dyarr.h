#pragma once

#include "memory/allocator.h"
#include "collections/arr_type.h"

namespace nk::cl {
    template <IArrT>
    class arr;

    template <IArrT T>
    class dyarr {
    public:
        dyarr();

        dyarr(dyarr&& other);
        dyarr& operator=(dyarr&& other);

        // template <IArr<T> Arr>
        // dyarr(Arr& other);
        // template <IArr<T> Arr>
        // dyarr(Arr&& other);

        dyarr(const dyarr&) = delete;
        dyarr& operator=(const dyarr&) = delete;

        ~dyarr();

        T& operator[](const u64 index);
        const T& operator[](const u64 index) const;

        T& _dyarr_at(const u64 index);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        T& _dyarr_at(cstr file, u32 line, const u64 index);
#endif

        const T& dyarr_at_const(const u64 index) const;

        void _dyarr_init(mem::Allocator* allocator, u64 capacity);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_init(cstr file, u32 line, mem::Allocator* allocator, u64 capacity);
#endif

        void _dyarr_init_len(mem::Allocator* allocator, u64 capacity, u64 length);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_init_len(cstr file, u32 line, mem::Allocator* allocator, u64 capacity, u64 length);
#endif

        void _dyarr_init_own(mem::Allocator* allocator, u64 capacity);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_init_own(cstr file, u32 line, mem::Allocator* allocator, u64 capacity);
#endif

        void _dyarr_init_own_len(mem::Allocator* allocator, u64 capacity, u64 length);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_init_own_len(cstr file, u32 line, mem::Allocator* allocator, u64 capacity, u64 length);
#endif

        void _dyarr_init_list(mem::Allocator* allocator, std::initializer_list<T> list);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_init_list(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list);
#endif

        void _dyarr_init_list_own(mem::Allocator* allocator, std::initializer_list<T> list);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_init_list_own(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list);
#endif

        //         void _dyarr_init_data(mem::Allocator* allocator, T* data, u64 length);
        // #if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        //         void _dyarr_init_data(cstr file, u32 line, mem::Allocator* allocator, T* data, u64 length);
        // #endif

        //         void _dyarr_init_data_own(mem::Allocator* allocator, T* data, u64 length);
        // #if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        //         void _dyarr_init_data_own(cstr file, u32 line, mem::Allocator* allocator, T* data, u64 length);
        // #endif

        void _dyarr_clear();
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_clear(cstr file, u32 line);
#endif

        void _dyarr_shutdown();
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_shutdown(cstr file, u32 line);
#endif

        T& dyarr_first();
        const T& dyarr_first() const;

        T& dyarr_last();
        const T& dyarr_last() const;

        void _dyarr_push(T& value);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_push(cstr file, u32 line, T& value);
#endif

        void _dyarr_push_ptr(T value)
            requires std::is_pointer_v<T>;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_push_ptr(cstr file, u32 line, T value)
            requires std::is_pointer_v<T>;
#endif

        void _dyarr_push_copy(const T& value);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_push_copy(cstr file, u32 line, const T& value);
#endif

        void _dyarr_insert(u64 index, T& value);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_insert(cstr file, u32 line, u64 index, T& value);
#endif

        void _dyarr_insert_ptr(u64 index, T value)
            requires std::is_pointer_v<T>;
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_insert_ptr(cstr file, u32 line, u64 index, T value)
            requires std::is_pointer_v<T>;
#endif

        void _dyarr_insert_copy(u64 index, const T& value);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_insert_copy(cstr file, u32 line, u64 index, const T& value);
#endif

        void _dyarr_resize(u64 length);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _dyarr_resize(cstr file, u32 line, u64 length);
#endif

        void dyarr_reset() { m_length = 0; }

        std::optional<T> dyarr_pop();
        std::optional<T> dyarr_remove(u64 index);

        T* data() { return m_data; }
        u64 length() const { return m_length; }
        u64 capacity() const { return m_capacity; }
        bool empty() const { return m_length == 0; }
        mem::Allocator* allocator() { return m_allocator; }
        bool owns_allocator() const { return m_own_allocator; }

    private:
        void grow(u64 capacity);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void grow(cstr file, u32 line, u64 capacity);
#endif

        T* m_data;
        u64 m_length;
        u64 m_capacity;
        mem::Allocator* m_allocator;
        bool m_own_allocator;

        friend class arr<T>;
    };

    template <IArrT T>
    dyarr<T>::dyarr()
        : m_data{nullptr},
          m_length{0},
          m_capacity{0},
          m_allocator{nullptr},
          m_own_allocator{false} {}

    template <IArrT T>
    dyarr<T>::dyarr(dyarr&& other)
        : m_data{other.m_data},
          m_length{other.m_length},
          m_capacity{other.m_capacity},
          m_allocator{other.m_allocator},
          m_own_allocator{other.m_own_allocator} {
        other.m_data = nullptr;
        other.m_length = 0;
        other.m_capacity = 0;
        other.m_allocator = nullptr;
        other.m_own_allocator = false;
    }

    template <IArrT T>
    dyarr<T>& dyarr<T>::operator=(dyarr&& other) {
        m_data = other.m_data;
        m_length = other.m_length;
        m_capacity = other.m_capacity;
        m_allocator = other.m_allocator;
        m_own_allocator = other.m_own_allocator;

        other.m_data = nullptr;
        other.m_length = 0;
        other.m_capacity = 0;
        other.m_allocator = nullptr;
        other.m_own_allocator = false;

        return *this;
    }

    // template <IArrT T>
    // template <IArr<T> Arr>
    // dyarr<T>::dyarr(Arr& other)
    //     : m_data{other.data()},
    //       m_length{other.length()},
    //       m_capacity{other.length()},
    //       m_allocator{other.allocator()},
    //       m_own_allocator{other.owns_allocator()} {
    //     other.m_data = nullptr;
    //     other.m_length = 0;
    //     other.m_capacity = 0;
    //     other.m_allocator = nullptr;
    //     other.m_own_allocator = false;
    // }

    // template <IArrT T>
    // template <IArr<T> Arr>
    // dyarr<T>::dyarr(Arr&& other)
    // {}

    template <IArrT T>
    dyarr<T>::~dyarr() {
        if (m_allocator != nullptr) {
            _dyarr_clear();
            return;
        }
        WarnLogIf(m_data != nullptr, "nk::cl::~dyarr not correctly freed.");
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
    T& dyarr<T>::_dyarr_at(const u64 index) {
        if (index >= m_length) {
            if (m_length >= m_capacity)
                grow(index + 1);

            m_length = index + 1;
        }

        return m_data[index];
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    T& dyarr<T>::_dyarr_at(cstr file, u32 line, const u64 index) {
        if (index >= m_length) {
            if (m_length >= m_capacity)
                grow(file, line, index + 1);
            m_length = index + 1;
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
    void dyarr<T>::_dyarr_init(mem::Allocator* allocator, u64 capacity) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        grow(capacity);
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_init(cstr file, u32 line, mem::Allocator* allocator, u64 capacity) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        grow(file, line, capacity);
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_init_len(mem::Allocator* allocator, u64 capacity, u64 length) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        grow(capacity);
        m_length = length;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_init_len(cstr file, u32 line, mem::Allocator* allocator, u64 capacity, u64 length) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        grow(file, line, capacity);
        m_length = length;
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_init_own(mem::Allocator* allocator, u64 capacity) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        m_own_allocator = true;
        grow(capacity);
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_init_own(cstr file, u32 line, mem::Allocator* allocator, u64 capacity) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        m_own_allocator = true;
        grow(file, line, capacity);
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_init_own_len(mem::Allocator* allocator, u64 capacity, u64 length) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        m_own_allocator = true;
        grow(capacity);
        m_length = length;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_init_own_len(cstr file, u32 line, mem::Allocator* allocator, u64 capacity, u64 length) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        m_own_allocator = true;
        grow(file, line, capacity);
        m_length = length;
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_init_list(mem::Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        grow(list.size());

        if (list.size() == 0)
            return;

        if constexpr (std::is_trivially_copyable_v<T> || std::is_same_v<T, cstr>) {
            std::memcpy(m_data, list.begin(), list.size() * sizeof(T));
        } else {
            std::uninitialized_move(list.begin(), list.end(), m_data);
        }

        m_length = list.size();
    }
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_init_list(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        grow(file, line, list.size());

        if (list.size() == 0)
            return;

        if constexpr (std::is_trivially_copyable_v<T> || std::is_same_v<T, cstr>) {
            std::memcpy(m_data, list.begin(), list.size() * sizeof(T));
        } else {
            std::uninitialized_move(list.begin(), list.end(), m_data);
        }

        m_length = list.size();
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_init_list_own(mem::Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        m_own_allocator = true;
        grow(list.size());

        if (list.size() == 0)
            return;

        if constexpr (std::is_trivially_copyable_v<T> || std::is_same_v<T, cstr>) {
            std::memcpy(m_data, list.begin(), list.size() * sizeof(T));
        } else {
            std::uninitialized_move(list.begin(), list.end(), m_data);
        }

        m_length = list.size();
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_init_list_own(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);
        m_allocator = allocator;
        m_own_allocator = true;
        grow(file, line, list.size());

        if (list.size() == 0)
            return;

        if constexpr (std::is_trivially_copyable_v<T> || std::is_same_v<T, cstr>) {
            std::memcpy(m_data, list.begin(), list.size() * sizeof(T));
        } else {
            std::uninitialized_move(list.begin(), list.end(), m_data);
        }

        m_length = list.size();
    }
#endif

    //     void _dyarr_init_data(mem::Allocator* allocator, T* data, u64 length);
    // #if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    //     void _dyarr_init_data(cstr file, u32 line, mem::Allocator* allocator, T* data, u64 length);
    // #endif

    //     void _dyarr_init_data_own(mem::Allocator* allocator, T* data, u64 length);
    // #if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    //     void _dyarr_init_data_own(cstr file, u32 line, mem::Allocator* allocator, T* data, u64 length);
    // #endif

    template <IArrT T>
    void dyarr<T>::_dyarr_clear() {
        if (m_data == nullptr)
            return;

        if (m_allocator == nullptr) {
            ErrorLogIf(m_capacity > 0, "nk::cl::dyarr::dyarr_clear Trying to clear array with no allocator, initialize.");
            return;
        }

        if (m_capacity > 0) {
            if constexpr (std::is_class_v<T>) {
                for (u64 i = 0; i < m_length; i++) {
                    m_data[i].~T();
                }
            }

            m_allocator->free_lot_t(T, m_data, m_capacity);
        }

        m_data = nullptr;
        m_length = 0;
        m_capacity = 0;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_clear(cstr file, u32 line) {
        if (m_data == nullptr)
            return;

        if (m_allocator == nullptr) {
            ErrorLogIf(m_capacity > 0, "nk::cl::dyarr::dyarr_clear Trying to clear array with no allocator, initialize.");
            return;
        }

        if (m_capacity > 0) {
            if constexpr (std::is_class_v<T>) {
                for (u64 i = 0; i < m_length; i++) {
                    m_data[i].~T();
                }
            }

            m_allocator->_free_lot_t<T>(file, line, m_data, m_capacity);
        }

        m_data = nullptr;
        m_length = 0;
        m_capacity = 0;
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_shutdown() {
        _dyarr_clear();

        if (!m_own_allocator) {
            m_allocator = nullptr;
            return;
        }

        native_deconstruct(mem::Allocator, m_allocator);
        m_allocator = nullptr;
        m_own_allocator = false;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_shutdown(cstr file, u32 line) {
        _dyarr_clear(file, line);

        if (!m_own_allocator) {
            m_allocator = nullptr;
            return;
        }

        native_deconstruct(mem::Allocator, m_allocator);
        m_allocator = nullptr;
        m_own_allocator = false;
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
    void dyarr<T>::_dyarr_push(T& value) {
        if (m_length >= m_capacity)
            grow(m_capacity);

        m_data[m_length] = std::move(value);
        m_length++;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_push(cstr file, u32 line, T& value) {
        if (m_length >= m_capacity)
            grow(file, line, m_capacity);

        m_data[m_length] = std::move(value);
        m_length++;
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_push_ptr(T value)
        requires std::is_pointer_v<T>
    {
        if (m_length >= m_capacity)
            grow(m_capacity);

        m_data[m_length] = value;
        m_length++;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_push_ptr(cstr file, u32 line, T value)
        requires std::is_pointer_v<T>
    {
        if (m_length >= m_capacity)
            grow(file, line, m_capacity);

        m_data[m_length] = value;
        m_length++;
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_push_copy(const T& value) {
        if (m_length >= m_capacity)
            grow(m_capacity);

        m_data[m_length] = value;
        m_length++;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_push_copy(cstr file, u32 line, const T& value) {
        if (m_length >= m_capacity)
            grow(file, line, m_capacity);

        m_data[m_length] = value;
        m_length++;
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_insert(u64 index, T& value) {
        if (index >= m_length) {
            if (m_length >= m_capacity)
                grow(index + 1);
            m_length = index + 1;
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = std::move(value);
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_insert(cstr file, u32 line, u64 index, T& value) {
        if (index >= m_length) {
            if (m_length >= m_capacity)
                grow(file, line, index + 1);
            m_length = index + 1;
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = std::move(value);
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_insert_ptr(u64 index, T value)
        requires std::is_pointer_v<T>
    {
        if (index >= m_length) {
            if (m_length >= m_capacity)
                grow(index + 1);
            m_length = index + 1;
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = value;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_insert_ptr(cstr file, u32 line, u64 index, T value)
        requires std::is_pointer_v<T>
    {
        if (index >= m_length) {
            if (m_length >= m_capacity)
                grow(file, line, index + 1);
            m_length = index + 1;
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = value;
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_insert_copy(u64 index, const T& value) {
        if (index >= m_length) {
            if (m_length >= m_capacity)
                grow(index + 1);
            m_length = index + 1;
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = value;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_insert_copy(cstr file, u32 line, u64 index, const T& value) {
        if (index >= m_length) {
            if (m_length >= m_capacity)
                grow(file, line, index + 1);
            m_length = index + 1;
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = value;
    }
#endif

    template <IArrT T>
    void dyarr<T>::_dyarr_resize(u64 length) {
        if (length >= m_capacity)
            grow(length);

        if constexpr (std::is_class_v<T>) {
            if (m_length > length) {
                for (u64 i = length; i < m_length; i++) {
                    m_data[i].~T();
                }
            }
        }
        m_length = length;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::_dyarr_resize(cstr file, u32 line, u64 length) {
        if (length >= m_capacity)
            grow(file, line, length);

        if constexpr (std::is_class_v<T>) {
            if (m_length > length) {
                for (u64 i = length; i < m_length; i++) {
                    m_data[i].~T();
                }
            }
        }
        m_length = length;
    }
#endif

    template <IArrT T>
    std::optional<T> dyarr<T>::dyarr_pop() {
        if (m_length > 0) {
            m_length--;
            return std::move(m_data[m_length]);
        }

        return std::nullopt;
    }

    template <IArrT T>
    std::optional<T> dyarr<T>::dyarr_remove(u64 index) {
        if (index >= m_length) {
            WarnLog("nk::cl::dyarr::remove Index '{}' out of bounds! Length: {}", index, m_length);
            return std::nullopt;
        }

        T value = std::move(m_data[index]);
        memmove(&m_data[index], &m_data[index + 1], sizeof *(m_data) * (m_length - 1 - index));
        m_length--;

        return std::move(value);
    }

    template <IArrT T>
    void dyarr<T>::grow(u64 capacity) {
        if (capacity < m_capacity * 2) {
            capacity = m_capacity * 2;
        }

        if (capacity < 4) {
            capacity = 4;
        }

        T* data = m_allocator->allocate_lot_t(T, capacity);

        if (m_length > 0)
            mem::realocate_n(m_data, data, m_length);

        if (m_capacity > 0)
            m_allocator->free_lot_t(T, m_data, m_capacity);

        m_data = data;
        m_capacity = capacity;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void dyarr<T>::grow(cstr file, u32 line, u64 capacity) {
        if (capacity < m_capacity * 2) {
            capacity = m_capacity * 2;
        }

        if (capacity < 4) {
            capacity = 4;
        }

        T* data = m_allocator->_allocate_lot_t<T>(file, line, capacity);

        if (m_length > 0)
            mem::realocate_n(m_data, data, m_length);

        if (m_capacity > 0)
            m_allocator->_free_lot_t<T>(file, line, m_data, m_capacity);

        m_data = data;
        m_capacity = capacity;
    }
#endif
}

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #define dyarr_at(index) \
        _dyarr_at(__FILE__, __LINE__, (index))

    #define dyarr_init(allocator, capacity) \
        _dyarr_init(__FILE__, __LINE__, (allocator), (capacity))

    #define dyarr_init_len(allocator, capacity, length) \
        _dyarr_init_len(__FILE__, __LINE__, (allocator), (capacity), (length))

    #define dyarr_init_own(allocator, capacity) \
        _dyarr_init_own(__FILE__, __LINE__, (allocator), (capacity))

    #define dyarr_init_own_len(allocator, capacity, length) \
        _dyarr_init_own_len(__FILE__, __LINE__, (allocator), (capacity), (length))

    #define dyarr_init_list(allocator, ...) \
        _dyarr_init_list(__FILE__, __LINE__, (allocator), __VA_ARGS__)

    #define dyarr_init_list_own(allocator, ...) \
        _dyarr_init_list_own(__FILE__, __LINE__, (allocator), __VA_ARGS__)

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
        _dyarr_init((allocator), (capacity), (length))

    #define dyarr_init_own(allocator, capacity) \
        _dyarr_init_own((allocator), (capacity))

    #define dyarr_init_own_len(allocator, capacity, length) \
        _dyarr_init_own((allocator), (capacity), (length))

    #define dyarr_init_list(allocator, ...) \
        _dyarr_init_list((allocator), __VA_ARGS__)

    #define dyarr_init_list_own(allocator, ...) \
        _dyarr_init_list_own((allocator), __VA_ARGS__)

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
