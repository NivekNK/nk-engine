#pragma once

#include "memory/memory_type.h"

namespace nk::mem {
    class Allocator;

    template <typename A, typename... Args>
    concept IAllocator = std::derived_from<A, Allocator> &&
                         requires(A allocator, Args... args) {
                             { allocator.init(args...) } -> std::same_as<void>;
                         };

    class Allocator {
    public:
        Allocator();
        virtual ~Allocator();

        Allocator(const Allocator&) = delete;
        Allocator& operator=(Allocator&) = delete;

        Allocator(Allocator&&);
        Allocator& operator=(Allocator&&);

        template <typename A, typename... Args>
            requires IAllocator<A, Args...>
        Allocator* allocator_init_impl(Args&&... args) {
            A* allocator = static_cast<A*>(this);
            allocator->init(std::forward<Args>(args)...);
            return allocator;
        }

        template <typename A>
            requires IAllocator<A>
        Allocator* allocator_init_impl() {
            A* allocator = static_cast<A*>(this);
            allocator->init();
            return allocator;
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename A, typename... Args>
            requires IAllocator<A, Args...>
        Allocator* inner_allocator_init(cstr file, u32 line, cstr name, MemoryType::Value type, Args&&... args) {
            A* allocator = static_cast<A*>(this);
            allocator->init(std::forward<Args>(args)...);
            _allocator_init(file, line, name, type);
            return allocator;
        }

        template <typename A>
            requires IAllocator<A>
        Allocator* inner_allocator_init(cstr file, u32 line, cstr name, MemoryType::Value type) {
            A* allocator = static_cast<A*>(this);
            allocator->init();
            _allocator_init(file, line, name, type);
            return allocator;
        }
#endif

        template <typename T>
        T* allocate_t_impl() {
            return static_cast<T*>(allocate_raw_impl(sizeof(T), alignof(T)));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        T* inner_allocate_t(cstr file, u32 line) {
            return static_cast<T*>(inner_allocate_raw(file, line, sizeof(T), alignof(T)));
        }
#endif

        template <typename T>
        void free_t_impl(T* data) {
            free_raw_impl(data, sizeof(T));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        void inner_free_t(cstr file, u32 line, T* data) {
            inner_free_raw(file, line, data, sizeof(T));
        }
#endif

        template <typename T>
        T* allocate_lot_t_impl(const u64 lot) {
            // TODO: Check if alignment is correct
            return static_cast<T*>(allocate_raw_impl(sizeof(T) * lot, alignof(T)));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        T* inner_allocate_lot_t(cstr file, u32 line, const u64 lot) {
            // TODO: Check if alignment is correct
            return static_cast<T*>(inner_allocate_raw(file, line, sizeof(T) * lot, alignof(T)));
        }
#endif

        template <typename T>
        void free_lot_t_impl(T* data, const u64 lot) {
            free_raw_impl(data, sizeof(T) * lot);
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        void inner_free_lot_t(cstr file, u32 line, T* data, const u64 lot) {
            inner_free_raw(file, line, data, sizeof(T) * lot);
        }
#endif

        template <typename T, typename... Args>
        T* construct_t_impl(Args&&... args) {
            return new (allocate_raw_impl(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T, typename... Args>
        T* inner_construct_t(cstr file, u32 line, Args&&... args) {
            return new (inner_allocate_raw(file, line, sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
        }
#endif

        template <typename T, typename V>
        void deconstruct_t_impl(V* data) {
            if (data == nullptr)
                return;
            data->~V();
            free_raw_impl(data, sizeof(T));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T, typename V>
        void inner_deconstruct_t(cstr file, u32 line, V* data) {
            if (data == nullptr)
                return;
            data->~V();
            inner_free_raw(file, line, data, sizeof(T));
        }
#endif

        virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) = 0;
        virtual void free_raw_impl(void* const data, const u64 size_bytes) = 0;

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void* inner_allocate_raw(cstr file, u32 line, const u64 size_bytes, const u64 alignment);
        void inner_free_raw(cstr file, u32 line, void* const data, const u64 size_bytes);
#endif

        virtual cstr to_cstr() const = 0;

        u64 get_size_bytes() const { return size_bytes; }
        u64 get_used_bytes() const { return used_bytes; }
        u64 get_allocation_count() const { return allocation_count; }

        void* data;

    protected:
        u64 size_bytes;
        u64 used_bytes;
        u64 allocation_count;

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    private:
        u32 key = numeric::u32_max;

        void _allocator_init(cstr file, u32 line, cstr name, MemoryType::Value type);

        friend class MemorySystem;
#endif
    };
}

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #define allocator_init(AllocatorType, name, type, ...) \
        inner_allocator_init<AllocatorType>(__FILE__, __LINE__, name, type __VA_OPT__(, ) __VA_ARGS__)
    #define allocate_t(Type) \
        inner_allocate_t<Type>(__FILE__, __LINE__)
    #define free_t(Type, data) \
        inner_free_t<Type>(__FILE__, __LINE__, data)
    #define allocate_lot_t(Type, lot) \
        inner_allocate_lot_t<Type>(__FILE__, __LINE__, lot)
    #define free_lot_t(Type, data, lot) \
        inner_free_lot_t<Type>(__FILE__, __LINE__, data, lot)
    #define contruct_t(Type, ...) \
        inner_construct_t<Type>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
    #define deconstruct_t(Type, data) \
        inner_deconstruct_t<Type>(__FILE__, __LINE__, data)
    #define allocate_raw(size_bytes, alignment) \
        inner_allocate_raw(__FILE__, __LINE__, size_bytes, alignment)
    #define free_raw(data, size_bytes) \
        inner_free_raw(__FILE__, __LINE__, data, size_bytes)

#else

    #define allocator_init(AllocatorType, name, type, ...) \
        allocator_init_impl<AllocatorType>(__VA_ARGS__)
    #define allocate_t(Type) \
        allocate_t_impl<Type>()
    #define free_t(Type, data) \
        free_t_impl<Type>(data)
    #define allocate_lot_t(Type, lot) \
        allocate_lot_t_impl<Type>(lot)
    #define free_lot_t(Type, data, lot) \
        free_lot_t_impl<Type>(data, lot)
    #define contruct_t(Type, ...) \
        construct_t_impl<Type>(__VA_ARGS__)
    #define deconstruct_t(Type, data) \
        deconstruct_t_impl<Type>(data)
    #define allocate_raw(size_bytes, alignment) \
        allocate_raw_impl(size_bytes, alignment)
    #define free_raw(data, size_bytes) \
        free_raw_impl(data, size_bytes)

#endif
