#pragma once

#include "memory/memory_type.h"

namespace nk {
    /**
     *  # How to implement a Allocator
     *
     *  ```cpp
     *  class MyAllocator : public Allocator {
     *  public:
     *      MyAllocator() : Allocator() {}
     *      virtual ~MyAllocator() override;
     *
     *      MyAllocator(MyAllocator&&);
     *      MyAllocator& operator=(MyAllocator&&);
     *
     *      virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) override;
     *      virtual void free_raw_impl(void* const ptr, const u64 size_bytes) override;
     *  }
     *  ```
     */
    class Allocator {
    public:
        Allocator();
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
            free_raw_impl(ptr, sizeof(T));
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
            free_raw_impl(ptr, sizeof(T) * lot);
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
            free_raw_impl(ptr, sizeof(T));
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

        virtual cstr to_cstr() const = 0;

        inline cstr c_name() const { return m_name.c_str(); }
        virtual str name() const { return m_name; }
        inline MemoryTypeValue memory_type() const { return m_type; }

        inline u32 index() const { return m_index; }
        inline void set_index(const u64 index) { m_index = index; }
#endif

        virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) = 0;
        virtual void free_raw_impl(void* const ptr, const u64 size_bytes) = 0;

    protected:
#if !defined(NK_RELEASE)
        void private_allocator_init(
            const u64 size_bytes, void* const start, str name, str file, const u32 line, const MemoryTypeValue memory_type);
#else
        void private_allocator_init(const u64 size_bytes, void* const start);
#endif

        u64 m_size_bytes;
        u64 m_used_bytes;
        u64 m_allocation_count;

        void* m_start;

#if !defined(NK_RELEASE)
        str m_name = "undefined";
        MemoryTypeValue m_type = MemoryType::None;
        u64 m_index;
#endif
    };

#define _NK_EXPAND_PARAMETERS(...) __VA_OPT__(, ) __VA_ARGS__
#define _NK_ESPAND_FUNC(...)       __VA_ARGS__

#if !defined(NK_RELEASE)
    #define NK_DEFINE_ALLOCATOR_INIT(allocator, size_bytes_name, start_name, parameters, func)                                             \
        allocator(const allocator&) = delete;                                                                                              \
        allocator& operator=(allocator&) = delete;                                                                                         \
        virtual cstr to_cstr() const override { return #allocator; }                                                                       \
        void allocator_init_impl(str name, str file, const u32 line, const MemoryTypeValue memory_type _NK_EXPAND_PARAMETERS parameters) { \
            _NK_ESPAND_FUNC func                                                                                                           \
                private_allocator_init(size_bytes_name, start_name, name, file, line, memory_type);                                        \
        }
#else
    #define NK_DEFINE_ALLOCATOR_INIT(allocator, size_bytes_name, start_name, parameters, func) \
        allocator(const allocator&) = delete;                                                  \
        allocator& operator=(allocator&) = delete;                                             \
        void allocator_init_impl(_NK_EXPAND_PARAMETERS parameters) {                           \
            _NK_ESPAND_FUNC func                                                               \
                private_allocator_init(size_bytes_name, start_name);                           \
        }
#endif

#if !defined(NK_RELEASE)
    #define allocator_init(name, memory_type, ...) \
        allocator_init_impl(name, __FILE__, __LINE__, memory_type __VA_OPT__(, ) __VA_ARGS__)
    #define allocate_raw(size_bytes, alignment) \
        memory_manager_allocate_raw_impl(size_bytes, alignment, __FILE__, __LINE__)
    #define allocate_t(T) \
        allocate_t_impl<T>(__FILE__, __LINE__)
    #define allocate_lot(T, lot) \
        allocate_lot_impl<T>(lot, __FILE__, __LINE__)
    #define allocate_raw_t(T, size_bytes) \
        allocate_raw_t_impl<T>(size_bytes, __FILE__, __LINE__)
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
#else
    #define allocator_init(name, memory_type, ...) \
        allocator_init_impl(__VA_ARGS__)
    #define allocate_raw(size_bytes, alignment) \
        allocate_raw_impl(size_bytes, alignment)
    #define allocate_t(T) \
        allocate_t_impl<T>()
    #define allocate_lot(T, lot) \
        allocate_lot_impl<T>(lot)
    #define allocate_raw_t(T, size_bytes) \
        allocate_raw_t_impl<T>(size_bytes)
    #define free_raw(ptr, size_bytes) \
        free_raw_impl(ptr, size_bytes)
    #define free_t(T, ptr) \
        free_t_impl<T>(ptr)
    #define free_lot(T, ptr, lot) \
        free_lot_impl<T>(ptr, lot)
    #define construct(T, ...) \
        construct_impl<T>(__VA_ARGS__)
    #define destroy(V, ptr) \
        destroy_impl<V>(ptr)
#endif
}
