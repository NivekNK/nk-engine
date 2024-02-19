#pragma once

#include "memory/memory_type.h"

namespace nk {
    class Allocator {
    public:
        Allocator()
            : m_size_bytes{0},
              m_used_bytes{0},
              m_allocation_count{0},
              m_start{nullptr} {}

        virtual ~Allocator();

        Allocator(const Allocator&) = delete;
        Allocator& operator=(Allocator&) = delete;

        Allocator(Allocator&&);
        Allocator& operator=(Allocator&&);

#if !defined(NK_RELEASE)
        template <typename T>
        inline T* allocate_t_impl(str file, const u32 line) {
            return static_cast<T*>(memory_manager_allocate_raw_impl(sizeof(T), alignof(T), file, line));
        }
#else
        template <typename T>
        inline T* allocate_t_impl() {
            return static_cast<T*>(allocate_raw_impl(sizeof(T), alignof(T)));
        }
#endif

#if !defined(NK_RELEASE)
        template <typename T>
        inline T* allocate_raw_t_impl(const u64 size_bytes, str file, const u32 line) {
            return static_cast<T*>(memory_manager_allocate_raw_impl(size_bytes, alignof(T), file, line));
        }
#else
        template <typename T>
        inline T* allocate_raw_t_impl(const u64 size_bytes) {
            return static_cast<T*>(allocate_raw_impl(size_bytes, alignof(T)));
        }
#endif

#if !defined(NK_RELEASE)
        template <typename T>
        inline T* allocate_lot_impl(const u64 lot, str file, const u32 line) {
            return static_cast<T*>(memory_manager_allocate_raw_impl(sizeof(T) * lot, alignof(T), file, line));
        }
#else
        template <typename T>
        inline T* allocate_lot_impl(const u64 lot) {
            return static_cast<T*>(allocate_raw_impl(sizeof(T) * lot, alignof(T)));
        }
#endif

#if !defined(NK_RELEASE)
        template <typename T>
        inline void free_t_impl(T* ptr, str file, const u32 line) {
            memory_manager_free_raw_impl(ptr, sizeof(T), file, line);
        }
#else
        template <typename T>
        inline void free_t_impl(T* ptr) {
            free_raw_impl(ptr);
        }
#endif

#if !defined(NK_RELEASE)
        template <typename T>
        inline void free_lot_impl(T* ptr, const u64 lot, str file, const u32 line) {
            memory_manager_free_raw_impl(ptr, sizeof(T) * lot, file, line);
        }
#else
        template <typename T>
        inline void free_lot_impl(T* ptr, const u64 lot) {
            free_raw_impl(ptr);
        }
#endif

#if !defined(NK_RELEASE)
        template <typename T, typename... Args>
        inline T* construct_impl(str file, const u32 line, Args&&... args) {
            return new (memory_manager_allocate_raw_impl(sizeof(T), alignof(T), file, line)) T(std::forward<Args>(args)...);
        }
#else
        template <typename T, typename... Args>
        inline T* construct_impl(Args&&... args) {
            return new (allocate_raw_impl(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
        }
#endif

#if !defined(NK_RELEASE)
        template <typename T, typename V>
        inline void destroy_impl(V* ptr, str file, const u32 line) {
            if (ptr == nullptr) {
                return;
            }

            ptr->~V();
            memory_manager_free_raw_impl(ptr, sizeof(T), file, line);
        }
#else
        template <typename T, typename V>
        inline void destroy_impl(V* ptr) {
            if (ptr == nullptr) {
                return;
            }

            ptr->~V();
            free_raw_impl(ptr);
        }
#endif

        inline u64 size() const { return m_size_bytes; }
        inline u64 used() const { return m_used_bytes; }
        inline u64 allocation_count() const { return m_allocation_count; }
        inline const void* start() const { return m_start; }

#if !defined(NK_RELEASE)
        void* memory_manager_allocate_raw_impl(
            const u64 size_bytes, const u64 alignment, str file, const u32 line);
        void memory_manager_free_raw_impl(
            void* const ptr, const u64 size_bytes, str file, const u32 line);
#endif

        virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) = 0;
        virtual void free_raw_impl(void* const ptr) = 0;

    protected:
#if !defined(NK_RELEASE)
        void allocator_init_impl(
            const u64 size_bytes, void* const start, str name, const MemoryTypeValue memory_type);
#else
        void allocator_init_impl(const u64 size_bytes, void* const start);
#endif

        u64 m_size_bytes;
        u64 m_used_bytes;
        u64 m_allocation_count;

        void* m_start;

#if !defined(NK_RELEASE)
        str m_name;
        MemoryTypeValue m_type;
#endif
    };

#if !defined(NK_RELEASE)
    #define allocator_init(size_bytes, start, name, memory_type) \
        allocator_init_impl(size_bytes, start, name, memory_type)
    #define allocate_raw(size_bytes, alignment) \
        memory_manager_allocate_raw_impl(size_bytes, alignment, __FILE__, __LINE__)
    #define allocate_t(T) \
        allocate_t_impl<T>(__FILE__, __LINE__)
    #define allocate_lot(T, lot) \
        allocate_lot_impl<T>(lot, __FILE__, __LINE__)
    #define free_raw(ptr, size_bytes) \
        memory_manager_free_raw_impl(ptr, size_bytes, __FILE__, __LINE__)
    #define free_t(T, ptr) \
        free_t_impl<T>(ptr, __FILE__, __LINE__)
    #define free_lot(T, ptr, lot) \
        free_lot_impl<T>(ptr, lot, __FILE__, __LINE__)
    #define construct(T, ...) \
        construct_impl<T>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
    #define destroy(V, ptr) \
        destroy_impl<V>(ptr, __FILE__, __LINE__)
    #define allocate_raw_t(T, size_bytes) \
        allocate_raw_t_impl<T>(size_bytes, __FILE__, __LINE__)
#else
    #define allocator_init(size_bytes, start, name, memory_type) \
        allocator_init_impl(size_bytes, start)
    #define allocate_raw(size_bytes, alignment) \
        allocate_raw_impl(size_bytes, alignment)
    #define allocate_t(T) \
        allocate_t_impl<T>()
    #define allocate_lot(T, lot) \
        allocate_lot_impl<T>(lot)
    #define free_raw(ptr, size_bytes) \
        free_raw_impl(ptr)
    #define free_t(T, ptr) \
        free_t_impl<T>(ptr)
    #define free_lot(T, ptr, lot) \
        free_lot_impl<T>(ptr, lot)
    #define construct(T, ...) \
        construct_impl<T>(__VA_ARGS__)
    #define destroy(V, ptr) \
        destroy_impl<V>(ptr)
    #define allocate_raw_t(T, size_bytes) \
        allocate_raw_t_impl<T>(size_bytes)
#endif
}
