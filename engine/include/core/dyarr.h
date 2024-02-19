#pragma once

#include "memory/allocator.h"
#include "core/result.h"

namespace nk {
    enum class DyarrError {
        EmptyDyarr,
        IndexBiggerThanLength,
    };

    template <typename T>
    class Dyarr {
    public:
        static_assert(!std::is_const_v<T>, "T must not be const");

        Dyarr()
            : m_data{nullptr},
              m_length{0},
              m_capacity{0},
              m_allocator{nullptr} {}

        ~Dyarr() {
            if (m_capacity > 0) {
                m_allocator->free_lot(T, m_data, m_capacity);
            }
        }

        Dyarr(const Dyarr&) = delete;
        Dyarr& operator=(const Dyarr&) = delete;

        Dyarr(Dyarr&& other) noexcept;
        Dyarr& operator=(Dyarr&& right) noexcept;

        void init(Allocator* allocator, const u64 initial_capacity, const u64 initial_length = 0) {
            m_data = nullptr;
            m_length = initial_length;
            m_capacity = 0;
            m_allocator = allocator;
            if (initial_capacity > 0)
                grow(initial_capacity);
        }

        void init_list(Allocator* allocator, std::initializer_list<T> data) {
            m_data = nullptr;
            m_length = data.size();
            m_capacity = 0;
            m_allocator = allocator;
            if (m_length > 0)
                grow(m_length);
            std::uninitialized_move(data.begin(), data.end(), m_data);
        }

        Result<T&, DyarrError> operator[](const u64 index);
        Result<const T&, DyarrError> operator[](const u64 index) const;

        Result<T&, DyarrError> head();
        Result<const T&, DyarrError> head() const;

        Result<T&, DyarrError> tail();
        Result<const T&, DyarrError> tail() const;

        void grow(u64 capacity);
        void clear();

        void push(const T& element);
        T& push_use();

        void insert(const T& element, const u64 index);
        T& insert_use(const u64 index);

        void remove_tail();
        void remove_swap_with_tail_at(const u64 index);
        void remove_at(const u64 index);

        void set_length(const u64 length);
        void set_capacity(const u64 capacity);

        inline u64 length() const { return m_length; }
        inline u64 capacity() const { return m_capacity; }
        inline bool empty() const { return m_length == 0; }
        inline T* data() { return m_data; }

    private:
        T* m_data;
        u64 m_length;
        u64 m_capacity;
        Allocator* m_allocator;
    };

    template <typename T>
    Dyarr<T>::Dyarr(Dyarr&& other) noexcept {
        m_data = other.m_data;
        m_length = other.m_length;
        m_capacity = other.m_capacity;
        m_allocator = other.m_allocator;

        other.m_data = nullptr;
        other.m_length = 0;
        other.m_capacity = 0;
        other.m_allocator = nullptr;
    }

    template <typename T>
    Dyarr<T>& Dyarr<T>::operator=(Dyarr&& right) noexcept {
        m_data = right.m_data;
        m_length = right.m_length;
        m_capacity = right.m_capacity;
        m_allocator = right.m_allocator;

        right.m_data = nullptr;
        right.m_length = 0;
        right.m_capacity = 0;
        right.m_allocator = nullptr;

        return *this;
    }

    template <typename T>
    Result<T&, DyarrError> Dyarr<T>::operator[](const u64 index) {
        ErrorLogIf(index >= m_length, "At index: {} when there is only {} length.", index, m_length);
        if (index >= m_length)
            return Err(DyarrError::IndexBiggerThanLength);
        return Ok(m_data[index]);
    }

    template <typename T>
    Result<const T&, DyarrError> Dyarr<T>::operator[](const u64 index) const {
        ErrorLogIf(index >= m_length, "At index: {} when there is only {} length.", index, m_length);
        if (index >= m_length)
            return Err(DyarrError::IndexBiggerThanLength);
        return Ok(m_data[index]);
    }

    template <typename T>
    Result<T&, DyarrError> Dyarr<T>::head() {
        ErrorLogIf(m_length <= 0, "Trying to get head when Dyarr is empty.");
        if (m_length <= 0)
            return Err(DyarrError::EmptyDyarr);
        return Ok(m_data[0]);
    }

    template <typename T>
    Result<const T&, DyarrError> Dyarr<T>::head() const {
        ErrorLogIf(m_length <= 0, "Trying to get head when Dyarr is empty.");
        if (m_length <= 0)
            return Err(DyarrError::EmptyDyarr);
        return Ok(m_data[0]);
    }

    template <typename T>
    Result<T&, DyarrError> Dyarr<T>::tail() {
        ErrorLogIf(m_length <= 0, "Trying to get tail when Dyarr is empty.");
        if (m_length <= 0)
            return Err(DyarrError::EmptyDyarr);
        return Ok(m_data[m_length - 1]);
    }

    template <typename T>
    Result<const T&, DyarrError> Dyarr<T>::tail() const {
        ErrorLogIf(m_length <= 0, "Trying to get tail when Dyarr is empty.");
        if (m_length <= 0)
            return Err(DyarrError::EmptyDyarr);
        return Ok(m_data[m_length - 1]);
    }

    template <typename T>
    void Dyarr<T>::grow(u64 capacity) {
        if (capacity < m_capacity * 2) {
            capacity = m_capacity * 2;
        } else if (capacity < 4) {
            capacity = 4;
        }

        T* data = m_allocator->allocate_lot(T, capacity);
        if (m_capacity > 0) {
            memcpy(data, m_data, m_capacity * sizeof(T));
            m_allocator->free_lot(T, m_data, m_capacity);
        }

        m_data = data;
        m_capacity = capacity;
    }

    template <typename T>
    void Dyarr<T>::clear() {
        m_length = 0;
    }

    template <typename T>
    void Dyarr<T>::push(const T& element) {
        if (m_length >= m_capacity)
            grow(m_capacity + 1);
        m_data[m_length++] = element;
    }

    template <typename T>
    T& Dyarr<T>::push_use() {
        if (m_length >= m_capacity)
            grow(m_capacity + 1);
        return m_data[m_length++];
    }

    template <typename T>
    void Dyarr<T>::insert(const T& element, const u64 index) {
        if (index >= m_capacity) {
            m_length = index + 1;
            grow(m_length);
            m_data[index] = element;
            return;
        }

        if (index >= m_length) {
            m_length = index + 1;
            m_data[index] = element;
            return;
        }

        m_length++;
        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        m_data[index] = element;
    }

    template <typename T>
    T& Dyarr<T>::insert_use(const u64 index) {
        if (index >= m_capacity) {
            m_length = index + 1;
            grow(m_length);
            return m_data[index];
        }

        if (index >= m_length) {
            m_length = index + 1;
            return m_data[index];
        }

        m_length++;
        memmove(&m_data[index + 1], &m_data[index], sizeof *(m_data) * (m_length - 1 - index));
        return m_data[index];
    }

    template <typename T>
    void Dyarr<T>::remove_tail() {
        WarnLogIf(m_length <= 0, "Trying to pop empty Dyarr.");
        if (m_length > 0)
            m_length--;
    }

    template <typename T>
    void Dyarr<T>::remove_swap_with_tail_at(const u64 index) {
        ErrorLogIf(index >= m_length, "Trying to remove and swap with tail at index: {} when there is only {} length.", index, m_length);
        if (index >= m_length)
            return;

        m_length--;
        m_data[index] = m_data[m_length];
    }

    template <typename T>
    void Dyarr<T>::remove_at(const u64 index) {
        WarnLogIf(index >= m_length, "Trying to remove at index: {} when there is only {} length.", index, m_length);
        if (index >= m_length)
            return;

        memmove(&m_data[index], &m_data[index + 1], sizeof *(m_data) * (m_length - 1 - index));
        m_length--;
    }

    template <typename T>
    void Dyarr<T>::set_length(const u64 length) {
        if (length > m_capacity)
            grow(length);
        m_length = length;
    }

    template <typename T>
    void Dyarr<T>::set_capacity(const u64 capacity) {
        if (capacity > m_capacity)
            grow(capacity);
    }
}
