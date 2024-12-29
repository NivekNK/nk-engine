#pragma once

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    #include "memory/memory_type.h"
#endif

namespace nk {
    class Allocator {
    public:
        Allocator();
        virtual ~Allocator();

        Allocator(const Allocator&) = delete;
        Allocator& operator=(Allocator&) = delete;

        Allocator(Allocator&&);
        Allocator& operator=(Allocator&&);

        template <typename A, typename... Args>
        Allocator* allocator_init_impl(Args&&... args) {
            return static_cast<A*>(this)->init(std::forward<Args>(args)...);
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename A, typename... Args>
        Allocator* _allocator_init_impl(cstr file, u32 line, cstr name, MemoryType::Value type, Args&&... args) {
            A* allocator = static_cast<A*>(this)->init(std::forward<Args>(args)...);
            _init_allocator(file, line, name, type);
            return allocator;
        }
#endif

        template <typename T>
        T* allocate_t_impl() {
            return static_cast<T*>(allocate_raw_impl(sizeof(T), alignof(T)));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        T* _allocate_t_impl(cstr file, u32 line) {
            return static_cast<T*>(_allocate_raw_impl(file, line, sizeof(T), alignof(T)));
        }
#endif

        template <typename T>
        void free_t_impl(T* data) {
            free_raw_impl(data, sizeof(T));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        void _free_t_impl(cstr file, u32 line, T* data) {
            _free_raw_impl(file, line, data, sizeof(T));
        }
#endif

        template <typename T>
        T* allocate_lot_impl(const u64 lot) {
            // TODO: Check if alignment is correct
            return static_cast<T*>(allocate_raw_impl(sizeof(T) * lot, alignof(T)));
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        T* _allocate_lot_impl(cstr file, u32 line, const u64 lot) {
            // TODO: Check if alignment is correct
            return static_cast<T*>(_allocate_raw_impl(file, line, sizeof(T) * lot, alignof(T)));
        }
#endif

        template <typename T>
        void free_lot_impl(T* data, const u64 lot) {
            free_raw_impl(data, sizeof(T) * lot);
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T>
        void _free_lot_impl(cstr file, u32 line, T* data, const u64 lot) {
            _free_raw_impl(file, line, data, sizeof(T) * lot);
        }
#endif

        template <typename T, typename... Args>
        T* construct_t_impl(Args&&... args) {
            return new (allocate_raw_impl(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
        }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
        template <typename T, typename... Args>
        T* _construct_t_impl(cstr file, u32 line, Args&&... args) {
            return new (_allocate_raw_impl(file, line, sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
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
        void _deconstruct_t_impl(cstr file, u32 line, V* data) {
            if (data == nullptr)
                return;
            data->~V();
            _free_raw_impl(file, line, data, sizeof(T));
        }
#endif

        virtual void* allocate_raw_impl(const u64 size_bytes, const u64 alignment) = 0;
        virtual void free_raw_impl(void* const data, const u64 size_bytes) = 0;

        virtual cstr to_cstr() const = 0;

        u64 size_bytes() const { return m_size_bytes; }
        u64 used_bytes() const { return m_used_bytes; }
        u64 allocation_count() const { return m_allocation_count; }
        void* data() { return m_data; }

    protected:
        u64 m_size_bytes;
        u64 m_used_bytes;
        u64 m_allocation_count;
        void* m_data;

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    private:
        u32 m_key = -1;

        void _init_allocator(cstr file, u32 line, cstr name, MemoryType::Value type);
        void* _allocate_raw_impl(cstr file, u32 line, const u64 size_bytes, const u64 alignment);
        void _free_raw_impl(cstr file, u32 line, void* const data, const u64 size_bytes);

        friend class MemorySystem;
#endif
    };
}

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM
    #define allocator_init(AllocatorType, ...) \
        _allocator_init_impl<AllocatorType>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
    #define allocate_t(Type) \
        _allocate_t_impl<Type>(__FILE__, __LINE__)
    #define free_t(Type, data) \
        _free_t_impl<Type>(__FILE__, __LINE__, data)
    #define allocate_lot(Type, lot) \
        _allocate_lot_impl<Type>(__FILE__, __LINE__, lot)
    #define free_lot(Type, data, lot) \
        _free_lot_impl<Type>(__FILE__, __LINE__, data, lot)
    #define construct_t(Type, ...) \
        _construct_t_impl<Type>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
    #define deconstruct_t(Type, data) \
        _deconstruct_t_impl<Type>(__FILE__, __LINE__, data)
#else
    #define allocator_init(AllocatorType, ...) \
        allocator_init_impl<AllocatorType>(__VA_ARGS__)
    #define allocate_t(Type) \
        allocate_t_impl<Type>()
    #define free_t(Type, data) \
        free_t_impl<Type>(data)
    #define allocate_lot(Type, lot) \
        allocate_lot_impl<Type>(lot)
    #define free_lot(Type, data, lot) \
        free_lot_impl<Type>(data, lot)
    #define construct_t(Type, ...) \
        construct_t_impl<Type>(__VA_ARGS__)
    #define deconstruct_t(Type, data) \
        deconstruct_t_impl<Type>(data)
#endif
