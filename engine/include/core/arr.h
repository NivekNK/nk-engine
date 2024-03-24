#pragma once

#include "memory/allocator.h"
#include "core/iterator.h"
#include "core/dyarr_type.h"

namespace nk {
    template <typename T>
    class Arr {
    public:
        static_assert(!std::is_const_v<T>, "T must not be const");

        Arr()
            : m_data{nullptr},
              m_length{0},
              m_allocator{nullptr},
              m_own_allocator{false} {}

        template <IDyarr<T> D>
        Arr(D& dyarr)
            : m_data{dyarr.data()},
              m_length{dyarr.length()},
              m_allocator{nullptr},
              m_own_allocator{false} {}

        ~Arr() { clear(); }

        Arr(const Arr&) = delete;
        Arr& operator=(const Arr&) = delete;

        Arr(Arr&& other) noexcept {
            m_data = other.m_data;
            m_length = other.m_length;
            m_allocator = other.m_allocator;
            m_own_allocator = other.m_own_allocator;

            other.m_data = nullptr;
            other.m_length = 0;
            other.m_allocator = nullptr;
            other.m_own_allocator = false;
        }

        Arr& operator=(Arr&& right) noexcept {
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

        void init(Allocator* allocator, const u64 length) {
            Assert(allocator != nullptr);

            m_length = length;
            m_allocator = allocator;
            m_data = m_allocator->allocate_lot(T, m_length);
            m_own_allocator = false;
        }

        void init_own(Allocator* allocator, const u64 length) {
            Assert(allocator != nullptr);

            m_length = length;
            m_allocator = allocator;
            m_data = m_allocator->allocate_lot(T, m_length);
            m_own_allocator = true;
        }

        void init_data(const u64 length, T* data) {
            m_length = length;
            m_data = data;
            m_allocator = nullptr;
            m_own_allocator = false;
        }

        void init_list(Allocator* allocator, std::initializer_list<T> list) {
            Assert(allocator != nullptr);

            m_length = list.size();
            m_allocator = allocator;
            m_data = m_allocator->allocate_lot(T, m_length);
            std::uninitialized_move(list.begin(), list.end(), m_data);
            m_own_allocator = false;
        }

        void init_list_own(Allocator* allocator, std::initializer_list<T> list) {
            Assert(allocator != nullptr);

            m_length = list.size();
            m_allocator = allocator;
            m_data = m_allocator->allocate_lot(T, m_length);
            std::uninitialized_move(list.begin(), list.end(), m_data);
            m_own_allocator = true;
        }

        using OptRefValue = std::optional<std::reference_wrapper<T>>;
        using ConstOptRefValue = std::optional<std::reference_wrapper<const T>>;

        OptRefValue operator[](const u64 index) {
            ErrorLogIf(index >= m_length, "At index: {} when there is only {} length.", index, m_length);
            if (index >= m_length)
                return std::nullopt;
            return std::ref(m_data[index]);
        }

        ConstOptRefValue operator[](const u64 index) const {
            ErrorLogIf(index >= m_length, "At index: {} when there is only {} length.", index, m_length);
            if (index >= m_length)
                return std::nullopt;
            return std::cref(m_data[index]);
        }

        OptRefValue first() {
            ErrorLogIf(m_length <= 0, "Trying to get first when Dyarr is empty.");
            if (m_length <= 0)
                return std::nullopt;
            return std::ref(m_data[0]);
        }

        ConstOptRefValue first() const {
            ErrorLogIf(m_length <= 0, "Trying to get first when Dyarr is empty.");
            if (m_length <= 0)
                return std::nullopt;
            return std::cref(m_data[0]);
        }

        OptRefValue last() {
            ErrorLogIf(m_length <= 0, "Trying to get last when Dyarr is empty.");
            if (m_length <= 0)
                return std::nullopt;
            return std::ref(m_data[m_length - 1]);
        }

        ConstOptRefValue last() const {
            ErrorLogIf(m_length <= 0, "Trying to get last when Dyarr is empty.");
            if (m_length <= 0)
                return std::nullopt;
            return std::cref(m_data[m_length - 1]);
        }

        void clear() {
            if (m_allocator == nullptr) {
                ErrorLogIf(m_length != 0, "Trying to clear array with no allocator, pass allocator.");
                return;
            }

            m_allocator->free_lot(T, m_data, m_length);
            if (m_own_allocator)
                delete m_allocator;
            reset();
        }

        void clear(Allocator* allocator) {
            WarnLogIf(m_length == 0, "Trying to clear array already empty.");
            allocator->free_lot(T, m_data, m_length);
            reset();
        }

        NK_DEFINE_ITERATOR(T, std::forward_iterator_tag);
        Iterator begin() { return Iterator{&m_data[0]}; }
        Iterator end() { return Iterator{&m_data[m_length]}; }
        Iterator begin() const { return Iterator{&m_data[0]}; }
        Iterator end() const { return Iterator{&m_data[m_length]}; }

        inline u64 length() const { return m_length; }
        inline bool empty() const { return m_length == 0; }
        inline T* data() { return m_data; }

    private:
        void reset() {
            m_data = nullptr;
            m_length = 0;
            m_allocator = nullptr;
            m_own_allocator = false;
        }

        T* m_data;
        u64 m_length;
        Allocator* m_allocator;
        bool m_own_allocator;
    };
}
