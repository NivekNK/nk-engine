#pragma once

#include "memory/allocator.h"
#include "core/arr_type.h"
#include "core/iterator.h"

namespace nk {
    template <IArrT T>
    class Arr {
    public:
        Arr();

        template <IArr<T> D>
        Arr(D& dyarr);

        Arr(Arr&& other);
        Arr& operator=(Arr&& right);

        Arr(const Arr&) = delete;
        Arr& operator=(const Arr&) = delete;

        ~Arr();

        void init(Allocator* allocator, const u64 length);
        void init_own(Allocator* allocator, const u64 length);
        void init_data(const u64 length, T* data);
        void init_list(Allocator* allocator, std::initializer_list<T> list);
        void init_list_own(Allocator* allocator, std::initializer_list<T> list);

        T& operator[](const u64 index);
        const T& operator[](const u64 index) const;

        T& first();
        const T& first() const;

        T& last();
        const T& last() const;

        void clear();
        void clear(Allocator* allocator);

        NK_DEFINE_ITERATOR(T, std::forward_iterator_tag);
        Iterator begin() { return Iterator{&m_data[0]}; }
        Iterator end() { return Iterator{&m_data[m_length]}; }
        Iterator begin() const { return Iterator{&m_data[0]}; }
        Iterator end() const { return Iterator{&m_data[m_length]}; }

        u64 length() const { return m_length; }
        bool empty() const { return m_length == 0; }
        T* data() { return m_data; }

    private:
        void reset();

        T* m_data;
        u64 m_length;
        Allocator* m_allocator;
        bool m_own_allocator;
    };

    template <IArrT T>
    Arr<T>::Arr()
        : m_data{nullptr},
          m_length{0},
          m_allocator{nullptr},
          m_own_allocator{false} {}

    template <IArrT T>
    template <IArr<T> D>
    Arr<T>::Arr(D& dyarr)
        : m_data{dyarr.data()},
          m_length{dyarr.length()},
          m_allocator{nullptr},
          m_own_allocator{false} {}

    template <IArrT T>
    Arr<T>::Arr(Arr&& other) {
        m_data = other.m_data;
        m_length = other.m_length;
        m_allocator = other.m_allocator;
        m_own_allocator = other.m_own_allocator;

        other.m_data = nullptr;
        other.m_length = 0;
        other.m_allocator = nullptr;
        other.m_own_allocator = false;
    }

    template <IArrT T>
    Arr<T>& Arr<T>::operator=(Arr&& right) {
        m_data = right.m_data;
        m_length = right.m_length;
        m_allocator = right.m_allocator;
        m_own_allocator = right.m_own_allocator;

        right.m_data = nullptr;
        right.m_length = 0;
        right.m_allocator = nullptr;
        right.m_own_allocator = false;

        return *this;
    }

    template <IArrT T>
    Arr<T>::~Arr() {
        if (m_allocator != nullptr) {
            clear();
            return;
        }
        ErrorLogIf(m_data != nullptr, "Arr::~Arr not correctly freed!");
    }

    template <IArrT T>
    void Arr<T>::init(Allocator* allocator, const u64 length) {
        Assert(allocator != nullptr);

        m_length = length;
        m_allocator = allocator;
        m_data = m_allocator->allocate_lot(T, m_length);

        if constexpr (std::is_arithmetic_v<T> || std::is_pointer_v<T> || std::is_enum_v<T>) {
            memset(m_data, 0, sizeof(T) * m_length);
        }

        m_own_allocator = false;
    }

    template <IArrT T>
    void Arr<T>::init_own(Allocator* allocator, const u64 length) {
        Assert(allocator != nullptr);

        m_length = length;
        m_allocator = allocator;
        m_data = m_allocator->allocate_lot(T, m_length);

        if constexpr (std::is_arithmetic_v<T>) {
            memset(m_data, 0, sizeof(T) * m_length);
        } else if constexpr (std::is_pointer_v<T>) {
            memset(m_data, nullptr, sizeof(T) * m_length);
        } else if constexpr (std::is_enum_v<T>) {
            memset(m_data, static_cast<T>(0), sizeof(T) * m_length);
        }

        m_own_allocator = true;
    }

    template <IArrT T>
    void Arr<T>::init_data(const u64 length, T* data) {
        m_length = length;
        m_data = data;
        m_allocator = nullptr;
        m_own_allocator = false;
    }

    template <IArrT T>
    void Arr<T>::init_list(Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);

        m_length = list.size();
        m_allocator = allocator;
        m_data = m_allocator->allocate_lot(T, m_length);
        std::uninitialized_move(list.begin(), list.end(), m_data);
        m_own_allocator = false;
    }

    template <IArrT T>
    void Arr<T>::init_list_own(Allocator* allocator, std::initializer_list<T> list) {
        Assert(allocator != nullptr);

        m_length = list.size();
        m_allocator = allocator;
        m_data = m_allocator->allocate_lot(T, m_length);
        std::uninitialized_move(list.begin(), list.end(), m_data);
        m_own_allocator = true;
    }

    template <IArrT T>
    T& Arr<T>::operator[](const u64 index) {
        AssertMsg(index < m_length, "nk::Arr::operator[] Index {} out of bounds! Length: {}", index, m_length);
        return m_data[index];
    }

    template <IArrT T>
    const T& Arr<T>::operator[](const u64 index) const {
        AssertMsg(index < m_length, "nk::Arr::operator[] Index {} out of bounds! Length: {}", index, m_length);
        return m_data[index];
    }

    template <IArrT T>
    T& Arr<T>::first() {
        AssertMsg(m_length > 0, "nk::Arr::first Array is empty!");
        return m_data[0];
    }

    template <IArrT T>
    const T& Arr<T>::first() const {
        AssertMsg(m_length > 0, "nk::Arr::first Array is empty!");
        return m_data[0];
    }

    template <IArrT T>
    T& Arr<T>::last() {
        AssertMsg(m_length > 0, "nk::Arr::last Array is empty!");
        return m_data[m_length - 1];
    }

    template <IArrT T>
    const T& Arr<T>::last() const {
        AssertMsg(m_length > 0, "nk::Arr::last Array is empty!");
        return m_data[m_length - 1];
    }

    template <IArrT T>
    void Arr<T>::clear() {
        if (m_allocator == nullptr) {
            ErrorLogIf(m_length > 0, "nk::Arr::clear Trying to clear array with no allocator, pass allocator.");
            return;
        }

        if (m_length > 0) {
            if constexpr(std::is_class_v<T>) {
                for (auto it = begin(); it != end(); it++) {
                    it->~T();
                }
            }

            m_allocator->free_lot(T, m_data, m_length);
        }

        if (m_own_allocator)
            delete m_allocator;

        reset();
    }

    template <IArrT T>
    void Arr<T>::clear(Allocator* allocator) {
        if (m_allocator != nullptr && m_length > 0) {
            WarnLog("nk::Arr::clear Trying to clear array with another allocator, freeing with its allocator.");

            if constexpr(std::is_class_v<T>) {
                for (auto it = begin(); it != end(); it++) {
                    it->~T();
                }
            }

            m_allocator->free_lot(T, m_data, m_length);
            return;
        }

        if (m_length > 0) {
            if constexpr(std::is_class_v<T>) {
                for (auto it = begin(); it != end(); it++) {
                    it->~T();
                }
            }

            allocator->free_lot(T, m_data, m_length);
        }

        reset();
    }

    template <IArrT T>
    void Arr<T>::reset() {
        m_data = nullptr;
        m_length = 0;
    }
}
