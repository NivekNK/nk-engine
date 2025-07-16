#pragma once

#include "memory/allocator.h"
#include "collections/arr_type.h"

namespace nk::cl {
    template <IArrT>
    class dyarr;

    template <IArrT T>
    class arr {
    public:
        arr();

        arr(arr&& other);
        arr& operator=(arr&& other);

        template <IDyarr<T> Dyarr>
        arr(Dyarr& other);
        template <IDyarr<T> Dyarr>
        arr(Dyarr&& other);

        arr(const arr&) = delete;
        arr& operator=(const arr&) = delete;

        ~arr();

        T& operator[](const u64 index);
        const T& operator[](const u64 index) const;

        T& at(const u64 index);
        const T& at(const u64 index) const;

        void _arr_init(mem::Allocator* allocator, u64 length);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_init(cstr file, u32 line, mem::Allocator* allocator, u64 length);
#endif

        void _arr_init_own(mem::Allocator* allocator, u64 length);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_init_own(cstr file, u32 line, mem::Allocator* allocator, u64 length);
#endif

        void _arr_init_list(mem::Allocator* allocator, std::initializer_list<T> list);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_init_list(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list);
#endif

        void _arr_init_list_own(mem::Allocator* allocator, std::initializer_list<T> list);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_init_list_own(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list);
#endif

        void _arr_clear();
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_clear(cstr file, u32 line);
#endif
        void _arr_clear(mem::Allocator* allocator);
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_clear(cstr file, u32 line, mem::Allocator* allocator);
#endif

        void _arr_shutdown();
#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_shutdown(cstr file, u32 line);
#endif

        T& arr_first();
        const T& arr_first() const;

        T& arr_last();
        const T& arr_last() const;

        T* data() { return m_data; }
        u64 length() const { return m_length; }
        bool empty() const { return m_length == 0; }
        mem::Allocator* allocator() { return m_allocator; }
        bool owns_allocator() const { return m_own_allocator; }

    private:
        T* m_data;
        u64 m_length;
        mem::Allocator* m_allocator;
        bool m_own_allocator;

        friend class dyarr<T>;
    };

    template <IArrT T>
    arr<T>::arr()
        : m_data{nullptr},
          m_length{0},
          m_allocator{nullptr},
          m_own_allocator{false} {}

    template <IArrT T>
    arr<T>::arr(arr&& other)
        : m_data{other.m_data},
          m_length{other.m_length},
          m_allocator{other.m_allocator},
          m_own_allocator{other.m_own_allocator} {
        other.m_data = nullptr;
        other.m_length = 0;
        other.m_allocator = nullptr;
        other.m_own_allocator = false;
    }

    template <IArrT T>
    arr<T>& arr<T>::operator=(arr&& other) {
        m_data = other.m_data;
        m_length = other.m_length;
        m_allocator = other.m_allocator;
        m_own_allocator = other.m_own_allocator;

        other.m_data = nullptr;
        other.m_length = 0;
        other.m_allocator = nullptr;
        other.m_own_allocator = false;

        return *this;
    }

    template <IArrT T>
    template <IDyarr<T> Dyarr>
    arr<T>::arr(Dyarr& other)
        : m_data{other.m_data},
          m_length{other.m_length},
          m_allocator{nullptr},
          m_own_allocator{false} {}

    template <IArrT T>
    template <IDyarr<T> Dyarr>
    arr<T>::arr(Dyarr&& other)
        : m_length{other.m_length},
          m_allocator{other.m_allocator},
          m_own_allocator{other.m_own_allocator} {
        if (m_length == 0) {
            m_data = nullptr;
            return;
        }

        m_data = m_allocator->allocate_lot_t(T, m_length);
        mem::realocate_n(other.m_data, m_data, m_length);
        m_allocator->free_lot_t(T, other.m_data, other.m_capacity);

        other.m_data = nullptr;
        other.m_length = 0;
        other.m_capacity = 0;
        other.m_allocator = nullptr;
        other.m_own_allocator = false;
    }

    template <IArrT T>
    arr<T>::~arr() {
        if (m_allocator != nullptr) {
            _arr_clear();
            return;
        }
        WarnLogIf(m_data != nullptr, "nk::cl::~arr not correctly freed.");
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
    void arr<T>::_arr_init(mem::Allocator* allocator, u64 length) {
        Assert(allocator != nullptr);

        m_length = length;
        m_allocator = allocator;
        m_data = m_allocator->allocate_lot_t(T, m_length);

        if constexpr (std::is_arithmetic_v<T>) {
            std::memset(m_data, 0, sizeof(T) * m_length);
        } else if constexpr (std::is_pointer_v<T>) {
            std::memset(m_data, nullptr, sizeof(T) * m_length);
        } else if constexpr (std::is_enum_v<T>) {
            std::memset(m_data, static_cast<T>(0), sizeof(T) * m_length);
        }

        m_own_allocator = false;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void arr<T>::_arr_init(cstr file, u32 line, mem::Allocator* allocator, u64 length) {
        Assert(allocator != nullptr);

        m_length = length;
        m_allocator = allocator;
        m_data = m_allocator->_allocate_lot_t<T>(file, line, m_length);

        if constexpr (std::is_arithmetic_v<T>) {
            std::memset(m_data, 0, sizeof(T) * m_length);
        } else if constexpr (std::is_pointer_v<T>) {
            std::memset(m_data, nullptr, sizeof(T) * m_length);
        } else if constexpr (std::is_enum_v<T>) {
            std::memset(m_data, static_cast<T>(0), sizeof(T) * m_length);
        }

        m_own_allocator = false;
    }
#endif

    template <IArrT T>
    void arr<T>::_arr_init_own(mem::Allocator* allocator, u64 length) {
        Assert(allocator != nullptr);

        m_length = length;
        m_allocator = allocator;
        m_data = m_allocator->allocate_lot_t(T, m_length);

        if constexpr (std::is_arithmetic_v<T>) {
            std::memset(m_data, 0, sizeof(T) * m_length);
        } else if constexpr (std::is_pointer_v<T>) {
            std::memset(m_data, nullptr, sizeof(T) * m_length);
        } else if constexpr (std::is_enum_v<T>) {
            std::memset(m_data, static_cast<T>(0), sizeof(T) * m_length);
        }

        m_own_allocator = true;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void arr<T>::_arr_init_own(cstr file, u32 line, mem::Allocator* allocator, u64 length) {
        Assert(allocator != nullptr);

        m_length = length;
        m_allocator = allocator;
        m_data = m_allocator->_allocate_lot_t<T>(file, line, m_length);

        if constexpr (std::is_arithmetic_v<T>) {
            std::memset(m_data, 0, sizeof(T) * m_length);
        } else if constexpr (std::is_pointer_v<T>) {
            std::memset(m_data, nullptr, sizeof(T) * m_length);
        } else if constexpr (std::is_enum_v<T>) {
            std::memset(m_data, static_cast<T>(0), sizeof(T) * m_length);
        }

        m_own_allocator = true;
    }
#endif

    template <IArrT T>
    void arr<T>::_arr_init_list(mem::Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);

        m_length = list.size();
        m_allocator = allocator;
        m_data = m_allocator->allocate_lot_t(T, m_length);
        std::uninitialized_move(list.begin(), list.end(), m_data);
        m_own_allocator = false;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void arr<T>::_arr_init_list(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);

        m_length = list.size();
        m_allocator = allocator;
        m_data = m_allocator->_allocate_lot_t<T>(file, line, m_length);
        std::uninitialized_move(list.begin(), list.end(), m_data);
        m_own_allocator = false;
    }
#endif

    template <IArrT T>
    void arr<T>::_arr_init_list_own(mem::Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);

        m_length = list.size();
        m_allocator = allocator;
        m_data = m_allocator->allocate_lot_t(T, m_length);
        std::uninitialized_move(list.begin(), list.end(), m_data);
        m_own_allocator = true;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void arr<T>::_arr_init_list_own(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);

        m_length = list.size();
        m_allocator = allocator;
        m_data = m_allocator->_allocate_lot_t<T>(file, line, m_length);
        std::uninitialized_move(list.begin(), list.end(), m_data);
        m_own_allocator = true;
    }
#endif

    template <IArrT T>
    void arr<T>::_arr_clear() {
        if (m_allocator == nullptr) {
            ErrorLogIf(m_length > 0, "nk::arr::arr_clear Trying to clear array with no allocator, pass allocator.");
            return;
        }

        if (m_length > 0) {
            if constexpr (std::is_class_v<T>) {
                for (u64 i = 0; i < m_length; i++) {
                    auto data = &m_data[i];
                    data->~T();
                }
            }
            m_allocator->free_lot_t(T, m_data, m_length);
        }

        m_data = nullptr;
        m_length = 0;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void arr<T>::_arr_clear(cstr file, u32 line) {
        if (m_allocator == nullptr) {
            if (m_length > 0)
                LoggingSystem::log(LoggingLevel::Error, file, line, "nk::arr::arr_clear Trying to clear array with no allocator, pass allocator.");
            return;
        }

        if (m_length > 0) {
            if constexpr (std::is_class_v<T>) {
                for (u64 i = 0; i < m_length; i++) {
                    auto data = &m_data[i];
                    data->~T();
                }
            }
            m_allocator->_free_lot_t<T>(file, line, m_data, m_length);
        }

        m_data = nullptr;
        m_length = 0;
    }
#endif

    template <IArrT T>
    void arr<T>::_arr_clear(mem::Allocator* allocator) {
        if (m_allocator != nullptr && m_length > 0) {
            WarnLog("nk::arr::arr_clear Trying to clear array with another allocator, freeing with its allocator.");

            if constexpr (std::is_class_v<T>) {
                for (u64 i = 0; i < m_length; i++) {
                    auto data = &m_data[i];
                    data->~T();
                }
            }

            m_allocator->free_lot_t(T, m_data, m_length);
            return;
        }

        if (m_length <= 0)
            return;

        if constexpr (std::is_class_v<T>) {
            for (u64 i = 0; i < m_length; i++) {
                auto data = &m_data[i];
                data->~T();
            }
        }

        allocator->free_lot_t(T, m_data, m_length);

        m_data = nullptr;
        m_length = 0;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void arr<T>::_arr_clear(cstr file, u32 line, mem::Allocator* allocator) {
        if (m_allocator != nullptr && m_length > 0) {
            LoggingSystem::log(LoggingLevel::Warning, file, line, "nk::arr::arr_clear Trying to clear array with another allocator, freeing with its allocator.");

            if constexpr (std::is_class_v<T>) {
                for (u64 i = 0; i < m_length; i++) {
                    auto data = &m_data[i];
                    data->~T();
                }
            }

            m_allocator->_free_lot_t<T>(file, line, m_data, m_length);
            return;
        }

        if (m_length <= 0)
            return;

        if constexpr (std::is_class_v<T>) {
            for (u64 i = 0; i < m_length; i++) {
                auto data = &m_data[i];
                data->~T();
            }
        }

        allocator->_free_lot_t<T>(file, line, m_data, m_length);

        m_data = nullptr;
        m_length = 0;
    }
#endif

    template <IArrT T>
    void arr<T>::_arr_shutdown() {
        _arr_clear();

        if (m_own_allocator)
            native_deconstruct(mem::Allocator, m_allocator);

        m_data = nullptr;
        m_length = 0;
        m_allocator = nullptr;
        m_own_allocator = false;
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    template <IArrT T>
    void arr<T>::_arr_shutdown(cstr file, u32 line) {
        _arr_clear(file, line);

        if (m_own_allocator)
            os::_native_deconstruct<mem::Allocator>(file, line, m_allocator);

        m_data = nullptr;
        m_length = 0;
        m_allocator = nullptr;
        m_own_allocator = false;
    }
#endif

    template <IArrT T>
    T& arr<T>::arr_first() {
        Assert(m_length > 0);
        return m_data[0];
    }

    template <IArrT T>
    const T& arr<T>::arr_first() const {
        Assert(m_length > 0);
        return m_data[0];
    }

    template <IArrT T>
    T& arr<T>::arr_last() {
        Assert(m_length > 0);
        return m_data[m_length - 1];
    }

    template <IArrT T>
    const T& arr<T>::arr_last() const {
        Assert(m_length > 0);
        return m_data[m_length - 1];
    }
}

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #define arr_init(allocator, length) \
        _arr_init(__FILE__, __LINE__, (allocator), (length))

    #define arr_init_own(allocator, length) \
        _arr_init_own(__FILE__, __LINE__, (allocator), (length))

    #define arr_init_list(allocator, list) \
        _arr_init_list(__FILE__, __LINE__, (allocator), (list))

    #define arr_init_list_own(allocator, list) \
        _arr_init_list_own(__FILE__, __LINE__, (allocator), (list))

    #define arr_clear() \
        _arr_clear(__FILE__, __LINE__)

    #define arr_clear_allocator(allocator) \
        _arr_clear(__FILE__, __LINE__, (allocator))

    #define arr_shutdown() \
        _arr_shutdown(__FILE__, __LINE__)

#else

    #define arr_init(allocator, length) \
        _arr_init((allocator), (length))

    #define arr_init_own(allocator, length) \
        _arr_init_own((allocator), (length))

    #define arr_init_list(allocator, list) \
        _arr_init_list((allocator), (list))

    #define arr_init_list_own(allocator, list) \
        _arr_init_list_own((allocator), (list))

    #define arr_clear() \
        _arr_clear()

    #define arr_clear_allocator(allocator) \
        _arr_clear((allocator))

    #define arr_shutdown() \
        _arr_shutdown()

#endif
