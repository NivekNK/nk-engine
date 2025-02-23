#pragma once

#include "memory/allocator.h"

namespace nk::cl {
    template <typename T>
    class arr {
    public:
        arr()
            : m_data{nullptr},
              m_length{0},
              m_allocator{nullptr},
              m_own_allocator{false} {}

        arr(arr&& other)
            : m_data{other.m_data},
              m_length{other.m_length},
              m_allocator{other.m_allocator},
              m_own_allocator{other.m_own_allocator} {
            other.m_data = nullptr;
            other.m_length = 0;
            other.m_allocator = nullptr;
            other.m_own_allocator = false;
        }

        arr& operator=(arr&& other) {
            m_data = other.m_data;
            m_length = other.m_length;
            m_allocator = other.m_allocator;
            m_own_allocator = other.m_own_allocator;

            other.m_data = nullptr;
            other.m_length = 0;
            other.m_allocator = nullptr;
            other.m_own_allocator = false;
        }

        template <typename Dyarr>
        arr(Dyarr& other)
            : m_data{other.data()},
              m_length{other.length()},
              m_allocator{nullptr},
              m_own_allocator{false} {}

        template <typename Dyarr>
        arr(Dyarr&& other)
            : m_data{other.data()},
              m_length{other.length()},
              m_allocator{other.allocator()},
              m_own_allocator{true} {
            other.reset_for_move();
        }

        arr(const arr&) = delete;
        arr& operator=(const arr&) = delete;

        ~arr() {
            if (m_allocator != nullptr) {
                _arr_clear();
                return;
            }
            ErrorLogIf(m_data != nullptr, "nk::cl::~arr not correctly freed.");
        }

        void _arr_init(mem::Allocator* allocator, u64 length) {
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
        void _arr_init(cstr file, u32 line, mem::Allocator* allocator, u64 length) {
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

        void _arr_init_own(mem::Allocator* allocator, u64 length) {
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
        void _arr_init_own(cstr file, u32 line, mem::Allocator* allocator, u64 length) {
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

        void _arr_init_data(u64 length, T* data) {
            m_length = length;
            m_data = data;
            m_allocator = nullptr;
            m_own_allocator = false;
        }

        void _arr_init_list(mem::Allocator* allocator, std::initializer_list<T> list) {
            Assert(allocator != nullptr);

            m_length = list.size();
            m_allocator = allocator;
            m_data = m_allocator->allocate_lot_t(T, m_length);
            std::uninitialized_move(list.begin(), list.end(), m_data);
            m_own_allocator = false;
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_init_list(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list) {
            Assert(allocator != nullptr);

            m_length = list.size();
            m_allocator = allocator;
            m_data = m_allocator->_allocate_lot_t<T>(file, line, m_length);
            std::uninitialized_move(list.begin(), list.end(), m_data);
            m_own_allocator = false;
        }
#endif

        void _arr_init_list_own(mem::Allocator* allocator, std::initializer_list<T> list) {
            Assert(allocator != nullptr);

            m_length = list.size();
            m_allocator = allocator;
            m_data = m_allocator->allocate_lot_t(T, m_length);
            std::uninitialized_move(list.begin(), list.end(), m_data);
            m_own_allocator = true;
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_init_list_own(cstr file, u32 line, mem::Allocator* allocator, std::initializer_list<T> list) {
            Assert(allocator != nullptr);

            m_length = list.size();
            m_allocator = allocator;
            m_data = m_allocator->_allocate_lot_t<T>(file, line, m_length);
            std::uninitialized_move(list.begin(), list.end(), m_data);
            m_own_allocator = true;
        }
#endif

        void _arr_clear() {
            if (m_allocator == nullptr) {
                ErrorLogIf(m_length > 0, "nk::arr::arr_clear Trying to clear array with no allocator, pass allocator.");
                return;
            }

            if (m_length > 0) {
                if constexpr (std::is_class_v<T>) {
                    if (u64 i = 0; i < m_length; i++) {
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
        void _arr_clear(cstr file, u32 line) {
            if (m_allocator == nullptr) {
                if (m_length > 0)
                    LoggingSystem::log(LoggingLevel::Error, file, line, "nk::arr::arr_clear Trying to clear array with no allocator, pass allocator.");
                return;
            }

            if (m_length > 0) {
                if constexpr (std::is_class_v<T>) {
                    if (u64 i = 0; i < m_length; i++) {
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

        void _arr_clear(mem::Allocator* allocator) {
            if (m_allocator != nullptr && m_length > 0) {
                WarnLog("nk::arr::arr_clear Trying to clear array with another allocator, freeing with its allocator.");

                if constexpr (std::is_class_v<T>) {
                    if (u64 i = 0; i < m_length; i++) {
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
                if (u64 i = 0; i < m_length; i++) {
                    auto data = &m_data[i];
                    data->~T();
                }
            }

            allocator->free_lot_t(T, m_data, m_length);

            m_data = nullptr;
            m_length = 0;
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_clear(cstr file, u32 line, mem::Allocator* allocator) {
            if (m_allocator != nullptr && m_length > 0) {
                LoggingSystem::log(LoggingLevel::Warning, file, line, "nk::arr::arr_clear Trying to clear array with another allocator, freeing with its allocator.");

                if constexpr (std::is_class_v<T>) {
                    if (u64 i = 0; i < m_length; i++) {
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
                if (u64 i = 0; i < m_length; i++) {
                    auto data = &m_data[i];
                    data->~T();
                }
            }

            allocator->_free_lot_t<T>(file, line, m_data, m_length);

            m_data = nullptr;
            m_length = 0;
        }
#endif

        void _arr_shutdown() {
            if (m_own_allocator)
                native_deconstruct(mem::Allocator, m_allocator);

            m_data = nullptr;
            m_length = 0;
            m_allocator = nullptr;
            m_own_allocator = false;
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void _arr_shutdown(cstr file, u32 line) {
            if (m_own_allocator)
                os::_native_deconstruct<mem::Allocator>(file, line, m_allocator);

            m_data = nullptr;
            m_length = 0;
            m_allocator = nullptr;
            m_own_allocator = false;
        }
#endif

        u64 get_length() const { return m_length; }
        T* get_data() { return m_data; }
        bool is_empty() const { return m_length == 0; }

    private:
        T* m_data;
        u64 m_length;
        mem::Allocator* m_allocator;
        bool m_own_allocator;
    };
}
