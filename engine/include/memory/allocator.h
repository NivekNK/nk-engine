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
        Allocator* _allocator_init(Args&&... args) {
            A* allocator = static_cast<A*>(this);
            allocator->init(std::forward<Args>(args)...);
            return allocator;
        }

        template <typename A>
            requires IAllocator<A>
        Allocator* _allocator_init() {
            A* allocator = static_cast<A*>(this);
            allocator->init();
            return allocator;
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename A, typename... Args>
            requires IAllocator<A, Args...>
        Allocator* _allocator_init_args(cstr file, u32 line, cstr name, MemoryType::Value type, Args&&... args) {
            A* allocator = static_cast<A*>(this);
            allocator->init(std::forward<Args>(args)...);
            _allocator_init(file, line, name, type);
            return allocator;
        }

        template <typename A>
            requires IAllocator<A>
        Allocator* _allocator_init_args(cstr file, u32 line, cstr name, MemoryType::Value type) {
            A* allocator = static_cast<A*>(this);
            allocator->init();
            _allocator_init(file, line, name, type);
            return allocator;
        }
#endif

        template <typename T>
        T* _allocate_t() {
            return static_cast<T*>(_allocate_raw(sizeof(T), alignof(T)));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        T* _allocate_t(cstr file, u32 line) {
            return static_cast<T*>(_allocate_raw(file, line, sizeof(T), alignof(T)));
        }
#endif

        template <typename T>
        bool _free_t(T* data) {
            return _free_raw(data, sizeof(T));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        bool _free_t(cstr file, u32 line, T* data) {
            return _free_raw(file, line, data, sizeof(T));
        }
#endif

        template <typename T>
        T* _allocate_lot_t(const u64 lot) {
            // TODO: Check if alignment is correct
            return static_cast<T*>(_allocate_raw(sizeof(T) * lot, alignof(T)));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        T* _allocate_lot_t(cstr file, u32 line, const u64 lot) {
            // TODO: Check if alignment is correct
            return static_cast<T*>(_allocate_raw(file, line, sizeof(T) * lot, alignof(T)));
        }
#endif

        template <typename T>
        bool _free_lot_t(T* data, const u64 lot) {
            return _free_raw(data, sizeof(T) * lot);
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        bool _free_lot_t(cstr file, u32 line, T* data, const u64 lot) {
            return _free_raw(file, line, data, sizeof(T) * lot);
        }
#endif

        template <typename T, typename... Args>
        T* _construct_t(Args&&... args) {
            return new (_allocate_raw(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T, typename... Args>
        T* _construct_t_args(cstr file, u32 line, Args&&... args) {
            return new (_allocate_raw(file, line, sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
        }
#endif

        template <typename T, typename V>
        bool _deconstruct_t(V* data) {
            if (data == nullptr)
                return false;
            data->~V();
            return _free_raw(data, sizeof(T));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T, typename V>
        bool _deconstruct_t(cstr file, u32 line, V* data) {
            if (data == nullptr)
                return false;
            data->~V();
            return _free_raw(file, line, data, sizeof(T));
        }
#endif

        virtual void* _allocate_raw(const u64 size_bytes, const u64 alignment) = 0;
        virtual bool _free_raw(void* const data, const u64 size_bytes) = 0;

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        void* _allocate_raw(cstr file, u32 line, const u64 size_bytes, const u64 alignment);
        bool _free_raw(cstr file, u32 line, void* const data, const u64 size_bytes);

        std::string_view _allocator_name();
#endif

        virtual cstr to_cstr() const = 0;

        u64 get_size_bytes() const { return m_size_bytes; }
        u64 get_used_bytes() const { return m_used_bytes; }
        u64 get_allocation_count() const { return m_allocation_count; }
        void* get_data() { return m_data; }

    protected:
        u64 m_size_bytes;
        u64 m_used_bytes;
        u64 m_allocation_count;
        void* m_data;

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    private:
        u32 m_key = numeric::u32_max;

        void _allocator_init(cstr file, u32 line, cstr name, MemoryType::Value type);

        friend class MemorySystem;
#endif
    };
}

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #define allocator_init(AllocatorType, name, type, ...) \
        _allocator_init_args<AllocatorType>(__FILE__, __LINE__, name, type __VA_OPT__(, ) __VA_ARGS__)
    #define allocate_t(Type) \
        _allocate_t<Type>(__FILE__, __LINE__)
    #define free_t(Type, data) \
        _free_t<Type>(__FILE__, __LINE__, data)
    #define allocate_lot_t(Type, lot) \
        _allocate_lot_t<Type>(__FILE__, __LINE__, lot)
    #define free_lot_t(Type, data, lot) \
        _free_lot_t<Type>(__FILE__, __LINE__, data, lot)
    #define construct_t(Type, ...) \
        _construct_t_args<Type>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
    #define deconstruct_t(Type, data) \
        _deconstruct_t<Type>(__FILE__, __LINE__, data)
    #define allocate_raw(size_bytes, alignment) \
        _allocate_raw(__FILE__, __LINE__, size_bytes, alignment)
    #define free_raw(data, size_bytes) \
        _free_raw(__FILE__, __LINE__, data, size_bytes)
    #define NK_ALLOCATOR_NAME(allocator) \
        allocator->_allocator_name()

#else

    #define allocator_init(AllocatorType, name, type, ...) \
        _allocator_init<AllocatorType>(__VA_ARGS__)
    #define allocate_t(Type) \
        _allocate_t<Type>()
    #define free_t(Type, data) \
        _free_t<Type>(data)
    #define allocate_lot_t(Type, lot) \
        _allocate_lot_t<Type>(lot)
    #define free_lot_t(Type, data, lot) \
        _free_lot_t<Type>(data, lot)
    #define construct_t(Type, ...) \
        _construct_t<Type>(__VA_ARGS__)
    #define deconstruct_t(Type, data) \
        _deconstruct_t<Type>(data)
    #define allocate_raw(size_bytes, alignment) \
        _allocate_raw(size_bytes, alignment)
    #define free_raw(data, size_bytes) \
        _free_raw(data, size_bytes)
    #define NK_ALLOCATOR_NAME(allocator) "Invalid"

#endif
