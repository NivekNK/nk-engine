#pragma once

#include "memory/allocator.h"
#include "core/arr_type.h"
#include "core/iterator.h"

namespace nk {
    template <IArrT T>
    class Dyarr {
    public:
        Dyarr();

        Dyarr(Dyarr&& other);
        Dyarr& operator=(Dyarr&& other);

        Dyarr(const Dyarr&) = delete;
        Dyarr& operator=(const Dyarr&) = delete;

        ~Dyarr();

        void init(Allocator* allocator, const u64 initial_capacity, const u64 initial_length = 0);
        void init_own(Allocator* allocator, const u64 initial_capacity, const u64 initial_length = 0);
        void init_list(Allocator* allocator, std::initializer_list<T> data);
        void init_list_own(Allocator* allocator, std::initializer_list<T> data);

        T& operator[](const u64 index);
        const T& operator[](const u64 index) const;

        T& first();
        const T& first() const;

        T& last();
        const T& last() const;

        void push(T& element);
        void push(T element) requires std::is_pointer_v<T>;
        void push_copy(const T& element);

        void insert(const u64 index, T& element);
        void insert(const u64 index, T element) requires std::is_pointer_v<T>;
        void insert_copy(const u64 index, const T& element);

        std::optional<T> pop();
        std::optional<T> remove(const u64 index);

        void clear();
        void clear(Allocator* allocator);

        void reset();
        void resize(u64 length);

        NK_DEFINE_ITERATOR(T, std::forward_iterator_tag);
        Iterator begin() { return Iterator{&m_data[0]}; }
        Iterator end() { return Iterator{&m_data[m_length]}; }
        Iterator begin() const { return Iterator{&m_data[0]}; }
        Iterator end() const { return Iterator{&m_data[m_length]}; }

        u64 length() const { return m_length; }
        u64 capacity() const { return m_capacity; }
        bool empty() const { return m_length == 0; }
        T* data() { return m_data; }

    private:
        void grow(u64 new_capacity);

        T* m_data;
        u64 m_length;
        u64 m_capacity;
        Allocator* m_allocator;
        bool m_own_allocator;
    };

    template <IArrT T>
    Dyarr<T>::Dyarr()
        : m_data{nullptr},
          m_length{0},
          m_capacity{0},
          m_allocator{nullptr},
          m_own_allocator{false} {}

    template <IArrT T>
    Dyarr<T>::Dyarr(Dyarr&& other) {
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
    }

    template <IArrT T>
    Dyarr<T>& Dyarr<T>::operator=(Dyarr&& other) {
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

    template <IArrT T>
    Dyarr<T>::~Dyarr() {
        ErrorLogIf(m_data != nullptr && m_allocator == nullptr, "Dyarr::~Dyarr Memory not cleaned!");

        if (m_allocator != nullptr) {
            clear();
        }
    }

    template <IArrT T>
    void Dyarr<T>::init(Allocator* allocator, const u64 initial_capacity, const u64 initial_length) {
        Assert(allocator != nullptr);

        m_data = nullptr;
        m_length = initial_length;
        m_capacity = 0;
        m_allocator = allocator;
        m_own_allocator = false;

        grow(initial_capacity);

        if constexpr (std::is_arithmetic_v<T> || std::is_pointer_v<T> || std::is_enum_v<T>) {
            memset(m_data, 0, sizeof(T) * m_capacity);
        }
    }

    template <IArrT T>
    void Dyarr<T>::init_own(Allocator* allocator, const u64 initial_capacity, const u64 initial_length) {
        Assert(allocator != nullptr);

        m_data = nullptr;
        m_length = initial_length;
        m_capacity = 0;
        m_allocator = allocator;
        m_own_allocator = true;

        if (initial_capacity > 0)
            grow(initial_capacity);

        if constexpr (std::is_arithmetic_v<T> || std::is_pointer_v<T> || std::is_enum_v<T>) {
            memset(m_data, 0, sizeof(T) * m_capacity);
        }
    }

    template <IArrT T>
    void Dyarr<T>::init_list(Allocator* allocator, std::initializer_list<T> data) {
        Assert(allocator != nullptr);

        m_data = nullptr;
        m_length = data.size();
        m_capacity = 0;
        m_allocator = allocator;
        m_own_allocator = false;

        if (m_length > 0)
            grow(m_length);

        std::uninitialized_move(data.begin(), data.end(), m_data);
    }

    template <IArrT T>
    void Dyarr<T>::init_list_own(Allocator* allocator, std::initializer_list<T> data) {
        Assert(allocator != nullptr);

        m_data = nullptr;
        m_length = data.size();
        m_capacity = 0;
        m_allocator = allocator;
        m_own_allocator = true;

        if (m_length > 0)
            grow(m_length);

        std::uninitialized_move(data.begin(), data.end(), m_data);
    }

    template <IArrT T>
    T& Dyarr<T>::operator[](const u64 index) {
        if (index >= m_length) {
            m_length = index + 1;

            if (m_length >= m_capacity)
                grow(m_length + 1);
        }

        return m_data[index];
    }

    template <IArrT T>
    const T& Dyarr<T>::operator[](const u64 index) const {
        AssertMsg(index < m_length, "nk::Dyarr::operator[] Index {} out of bounds! Length: {}", index, m_length);
        return m_data[index];
    }

    template <IArrT T>
    T& Dyarr<T>::first() {
        AssertMsg(m_length > 0, "nk::Dyarr::first Array is empty!");
        return m_data[0];
    }

    template <IArrT T>
    const T& Dyarr<T>::first() const {
        AssertMsg(m_length > 0, "nk::Dyarr::first Array is empty!");
        return m_data[0];
    }

    template <IArrT T>
    T& Dyarr<T>::last() {
        AssertMsg(m_length > 0, "nk::Dyarr::last Array is empty!");
        return m_data[m_length - 1];
    }

    template <IArrT T>
    const T& Dyarr<T>::last() const {
        AssertMsg(m_length > 0, "nk::Dyarr::last Array is empty!");
        return m_data[m_length - 1];
    }

    template <IArrT T>
    void Dyarr<T>::push(T& element) {
        if (m_length >= m_capacity)
            grow(m_capacity + 1);

        m_data[m_length] = std::move(element);
        m_length++;
    }

    template <IArrT T>
    void Dyarr<T>::push(T element) requires std::is_pointer_v<T> {
        if (m_length >= m_capacity)
            grow(m_capacity + 1);

        m_data[m_length] = element;
        m_length++;
    }

    template <IArrT T>
    void Dyarr<T>::push_copy(const T& element) {
        if (m_length >= m_capacity)
            grow(m_capacity + 1);

        m_data[m_length] = element;
        m_length++;
    }

    template <IArrT T>
    void Dyarr<T>::insert(const u64 index, T& element) {
        if (index >= m_length) {
            m_length = index + 1;

            if (m_length >= m_capacity)
                grow(m_length + 1);
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = std::move(element);
    }

    template <IArrT T>
    void Dyarr<T>::insert(const u64 index, T element) requires std::is_pointer_v<T> {
        if (index >= m_length) {
            m_length = index + 1;

            if (m_length >= m_capacity)
                grow(m_length + 1);
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = element;
    }

    template <IArrT T>
    void Dyarr<T>::insert_copy(const u64 index, const T& element) {
        if (index >= m_length) {
            m_length = index + 1;

            if (m_length >= m_capacity)
                grow(m_length + 1);
        } else {
            m_length++;
        }

        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = element;
    }

    template <IArrT T>
    std::optional<T> Dyarr<T>::pop() {
        if (m_length > 0) {
            m_length--;
            return std::move(m_data[m_length]);
        }

        return std::nullopt;
    }

    template <IArrT T>
    std::optional<T> Dyarr<T>::remove(const u64 index) {
        if (index >= m_length) {
            WarnLog("nk::Dyarr::remove Index {} out of bounds! Length: {}", index, m_length);
            return;
        }

        T value = std::move(m_data[index]);
        memmove(&m_data[index], &m_data[index + 1], sizeof *(m_data) * (m_length - 1 - index));
        m_length--;

        return std::move(value);
    }

    template <IArrT T>
    void Dyarr<T>::clear() {
        if (m_allocator == nullptr) {
            ErrorLogIf(m_capacity > 0, "nk::Dyarr::clear Trying to clear array with no allocator, pass allocator.");
            return;
        }

        if (m_capacity > 0) {
            if constexpr (std::is_class_v<T>) {
                for (auto it = begin(); it != end(); it++) {
                    it->~T();
                }
            }

            m_allocator->free_lot(T, m_data, m_capacity);
        }

        if (m_own_allocator)
            delete m_allocator;

        reset();
    }

    template <IArrT T>
    void Dyarr<T>::clear(Allocator* allocator) {
        if (m_allocator != nullptr && m_capacity > 0) {
            WarnLog("nk::Dyarr::clear Trying to clear array with another allocator, freeing with its allocator.");

            if constexpr (std::is_class_v<T>) {
                for (auto it = begin(); it != end(); it++) {
                    it->~T();
                }
            }

            m_allocator->free_lot(T, m_data, m_length);
            return;
        }

        if (m_capacity > 0) {
            if constexpr (std::is_class_v<T>) {
                for (auto it = begin(); it != end(); it++) {
                    it->~T();
                }
            }

            m_allocator->free_lot(T, m_data, m_capacity);
        }

        reset();
    }

    template <IArrT T>
    void Dyarr<T>::reset() {
        m_data = nullptr;
        m_length = 0;
        m_capacity = 0;
    }

    template <IArrT T>
    void Dyarr<T>::resize(u64 length) {
        if (length >= m_capacity)
            grow(length);

        m_length = length;
    }

    template <IArrT T>
    void Dyarr<T>::grow(u64 new_capacity) {
        if (new_capacity < m_capacity * 2) {
            new_capacity = m_capacity * 2;
        } else if (new_capacity < 4) {
            new_capacity = 4;
        }

        T* data = m_allocator->allocate_lot(T, new_capacity);
        if (m_capacity > 0) {
            memcpy(data, m_data, m_capacity * sizeof(T));
            m_allocator->free_lot(T, m_data, m_capacity);
        }

        m_data = data;
        m_capacity = new_capacity;
    }
}
