#pragma once

#include "memory/allocation_tracker.h"
#include "memory/memory_type.h"

namespace nk::mem {
    class Allocator;

    template <typename A, typename... Args>
    concept IAllocator = std::derived_from<A, Allocator> &&
                         requires(A& allocator, Args&&... args) {
                             allocator.init(std::forward<Args>(args)...);
                             requires(
                                 std::same_as<
                                     decltype(allocator.init(std::forward<Args>(args)...)),
                                     void> ||
                                 std::same_as<
                                     decltype(allocator.init(std::forward<Args>(args)...)),
                                     bool>);
                         };

    enum class AllocatorLifecycle : u8 {
        Uninitialized,
        Initialized,
        InitializationFailed,
        MovedFrom,
    };

    class Allocator {
    public:
        Allocator() noexcept;
        virtual ~Allocator();

        Allocator(const Allocator&) = delete;
        Allocator& operator=(const Allocator&) = delete;

        template <typename A, typename... Args>
            requires IAllocator<A, Args...>
        Allocator* _allocator_init_untracked(Args&&... args) {
            if (!_begin_initialization())
                return nullptr;

            A* allocator = static_cast<A*>(this);
            if (!_initialize_backend(*allocator, std::forward<Args>(args)...)) {
                _fail_initialization();
                return nullptr;
            }
            _complete_initialization(untracked);
            return allocator;
        }

        // Compatibility name for existing Release call sites.
        template <typename A, typename... Args>
            requires IAllocator<A, Args...>
        Allocator* _allocator_init(Args&&... args) {
            return _allocator_init_untracked<A>(std::forward<Args>(args)...);
        }

#if NK_MEMORY_TRACKING_ENABLED
        template <typename A, typename... Args>
            requires IAllocator<A, Args...>
        Allocator* _allocator_init_tracked(
            AllocationTracker& tracker,
            cstr file,
            u32 line,
            cstr name,
            u32 memory_type,
            Args&&... args) {
            if (!_begin_initialization())
                return nullptr;

            A* allocator = static_cast<A*>(this);
            if (!_initialize_backend(*allocator, std::forward<Args>(args)...)) {
                _fail_initialization();
                return nullptr;
            }
            if (!_complete_initialization(tracker, {
                .name = name,
                .implementation = allocator->to_cstr(),
                .memory_type = memory_type,
                .source = {file, line},
                .statistics = statistics(),
            })) {
                return nullptr;
            }
            return allocator;
        }
#endif

        template <typename T>
        T* _allocate_t() noexcept {
            return static_cast<T*>(_allocate_raw(sizeof(T), alignof(T)));
        }

        template <typename T>
        T* _allocate_zeroed_t() noexcept {
            return static_cast<T*>(_allocate_zeroed_raw(sizeof(T), alignof(T)));
        }

#if NK_MEMORY_TRACKING_ENABLED
        template <typename T>
        T* _allocate_t(cstr file, u32 line) noexcept {
            return static_cast<T*>(_allocate_raw(file, line, sizeof(T), alignof(T)));
        }

        template <typename T>
        T* _allocate_zeroed_t(cstr file, u32 line) noexcept {
            return static_cast<T*>(
                _allocate_zeroed_raw(file, line, sizeof(T), alignof(T)));
        }
#endif

        template <typename T>
        bool _free_t(T* data) noexcept {
            return _free_raw(data, sizeof(T));
        }

#if NK_MEMORY_TRACKING_ENABLED
        template <typename T>
        bool _free_t(cstr file, u32 line, T* data) noexcept {
            return _free_raw(file, line, data, sizeof(T));
        }
#endif

        template <typename T>
        T* _allocate_lot_t(const u64 count) noexcept {
            u64 size_bytes = 0;
            if (!_checked_type_size<T>(count, size_bytes))
                return nullptr;
            return static_cast<T*>(_allocate_raw(size_bytes, alignof(T)));
        }

        template <typename T>
        T* _allocate_zeroed_lot_t(const u64 count) noexcept {
            u64 size_bytes = 0;
            if (!_checked_type_size<T>(count, size_bytes))
                return nullptr;
            return static_cast<T*>(_allocate_zeroed_raw(size_bytes, alignof(T)));
        }

#if NK_MEMORY_TRACKING_ENABLED
        template <typename T>
        T* _allocate_lot_t(cstr file, u32 line, const u64 count) noexcept {
            u64 size_bytes = 0;
            if (!_checked_type_size<T>(count, size_bytes))
                return nullptr;
            return static_cast<T*>(_allocate_raw(file, line, size_bytes, alignof(T)));
        }

        template <typename T>
        T* _allocate_zeroed_lot_t(cstr file, u32 line, const u64 count) noexcept {
            u64 size_bytes = 0;
            if (!_checked_type_size<T>(count, size_bytes))
                return nullptr;
            return static_cast<T*>(
                _allocate_zeroed_raw(file, line, size_bytes, alignof(T)));
        }
#endif

        template <typename T>
        bool _free_lot_t(T* data, const u64 count) noexcept {
            u64 size_bytes = 0;
            if (!_checked_type_size<T>(count, size_bytes))
                return false;
            return _free_raw(data, size_bytes);
        }

#if NK_MEMORY_TRACKING_ENABLED
        template <typename T>
        bool _free_lot_t(cstr file, u32 line, T* data, const u64 count) noexcept {
            u64 size_bytes = 0;
            if (!_checked_type_size<T>(count, size_bytes))
                return false;
            return _free_raw(file, line, data, size_bytes);
        }
#endif

        template <typename T, typename... Args>
        T* _construct_t(Args&&... args) {
            void* storage = _allocate_raw(sizeof(T), alignof(T));
            if (storage == nullptr)
                return nullptr;
            return new (storage) T(std::forward<Args>(args)...);
        }

#if NK_MEMORY_TRACKING_ENABLED
        template <typename T, typename... Args>
        T* _construct_t_args(cstr file, u32 line, Args&&... args) {
            void* storage = _allocate_raw(file, line, sizeof(T), alignof(T));
            if (storage == nullptr)
                return nullptr;
            return new (storage) T(std::forward<Args>(args)...);
        }
#endif

        template <typename T, typename V>
        bool _deconstruct_t(V* data) noexcept {
            if (data == nullptr)
                return false;
            auto* typed_data = static_cast<T*>(data);
            typed_data->~T();
            return _free_raw(typed_data, sizeof(T));
        }

#if NK_MEMORY_TRACKING_ENABLED
        template <typename T, typename V>
        bool _deconstruct_t(cstr file, u32 line, V* data) noexcept {
            if (data == nullptr)
                return false;
            auto* typed_data = static_cast<T*>(data);
            typed_data->~T();
            return _free_raw(file, line, typed_data, sizeof(T));
        }
#endif

        void* _allocate_raw(u64 size_bytes, u64 alignment) noexcept;
        void* _allocate_zeroed_raw(u64 size_bytes, u64 alignment) noexcept;
        bool _free_raw(void* data, u64 size_bytes) noexcept;

#if NK_MEMORY_TRACKING_ENABLED
        void* _allocate_raw(cstr file, u32 line, u64 size_bytes, u64 alignment) noexcept;
        void* _allocate_zeroed_raw(
            cstr file,
            u32 line,
            u64 size_bytes,
            u64 alignment) noexcept;
        bool _free_raw(cstr file, u32 line, void* data, u64 size_bytes) noexcept;
#endif

#if NK_MEMORY_TRACKING_ENABLED
        bool attach_tracker(
            AllocationTracker& tracker,
            cstr name,
            u32 memory_type,
            SourceLocation source) noexcept;
        bool detach_tracker() noexcept;
#endif

        virtual cstr to_cstr() const noexcept = 0;

        cstr _allocator_name() const noexcept;
        bool is_initialized() const noexcept { return m_lifecycle == AllocatorLifecycle::Initialized; }
        bool is_tracked() const noexcept;
        AllocatorLifecycle lifecycle() const noexcept { return m_lifecycle; }
        AllocatorId allocator_id() const noexcept;

        u64 get_reserved_bytes() const noexcept { return m_reserved_bytes; }
        u64 get_size_bytes() const noexcept { return m_reserved_bytes; }
        u64 get_used_bytes() const noexcept { return m_used_bytes; }
        u64 get_peak_used_bytes() const noexcept { return m_peak_used_bytes; }
        u64 get_active_allocation_count() const noexcept { return m_active_allocations; }
        u64 get_allocation_count() const noexcept { return m_active_allocations; }
        void* get_data() noexcept { return m_data; }

        AllocatorStatistics statistics() const noexcept {
            return {
                .reserved_bytes = m_reserved_bytes,
                .used_bytes = m_used_bytes,
                .peak_used_bytes = m_peak_used_bytes,
                .active_allocations = m_active_allocations,
            };
        }

    protected:
        // Concrete allocators expose their own move operations so they can
        // release backend-specific state before delegating to this base.
        Allocator(Allocator&& other) noexcept;
        Allocator& operator=(Allocator&& other) noexcept;

        virtual void* _do_allocate(u64 size_bytes, u64 alignment) noexcept = 0;
        virtual bool _do_free(void* data, u64 size_bytes) noexcept = 0;

        void _update_peak() noexcept {
            if (m_used_bytes > m_peak_used_bytes)
                m_peak_used_bytes = m_used_bytes;
        }

        void _notify_reset(SourceLocation source) noexcept;
        bool _can_accept_move() const noexcept;

        u64 m_reserved_bytes;
        u64 m_used_bytes;
        u64 m_peak_used_bytes;
        u64 m_active_allocations;
        void* m_data;

    private:
        template <typename A, typename... Args>
        static bool _initialize_backend(A& allocator, Args&&... args) {
            if constexpr (std::same_as<
                              decltype(allocator.init(std::forward<Args>(args)...)),
                              bool>) {
                return allocator.init(std::forward<Args>(args)...);
            } else {
                allocator.init(std::forward<Args>(args)...);
                return true;
            }
        }

        template <typename T>
        static bool _checked_type_size(u64 count, u64& size_bytes) noexcept {
            constexpr u64 element_size = sizeof(T);
            if (count == 0 || count > numeric::u64_max / element_size) {
                size_bytes = 0;
                return false;
            }
            size_bytes = element_size * count;
            return true;
        }

        bool _begin_initialization() noexcept;
        void _fail_initialization() noexcept;
        void _complete_initialization(Untracked) noexcept;
#if NK_MEMORY_TRACKING_ENABLED
        bool _complete_initialization(
            AllocationTracker& tracker,
            const AllocatorDescriptor& descriptor) noexcept;
#endif
        void* _allocate_impl(SourceLocation source, u64 size_bytes, u64 alignment) noexcept;
        bool _free_impl(SourceLocation source, void* data, u64 size_bytes) noexcept;
        void _detach_tracker_unchecked() noexcept;
        void _move_from(Allocator& other) noexcept;
        void _reset_moved_from() noexcept;

        AllocatorLifecycle m_lifecycle;

#if NK_MEMORY_TRACKING_ENABLED
        AllocationTracker* m_tracker;
        AllocatorId m_allocator_id;
#endif
    };
}

#if NK_MEMORY_TRACKING_ENABLED

    #define allocator_init(AllocatorType, name, type, ...) \
        _allocator_init_tracked<AllocatorType>(nk::mem::default_allocation_tracker(), __FILE__, __LINE__, name, type __VA_OPT__(, ) __VA_ARGS__)
    #define allocator_init_tracked(AllocatorType, tracker, name, type, ...) \
        _allocator_init_tracked<AllocatorType>(tracker, __FILE__, __LINE__, name, type __VA_OPT__(, ) __VA_ARGS__)
    #define allocate_t(Type) \
        _allocate_t<Type>(__FILE__, __LINE__)
    #define allocate_zeroed_t(Type) \
        _allocate_zeroed_t<Type>(__FILE__, __LINE__)
    #define free_t(Type, data) \
        _free_t<Type>(__FILE__, __LINE__, data)
    #define allocate_lot_t(Type, lot) \
        _allocate_lot_t<Type>(__FILE__, __LINE__, lot)
    #define allocate_zeroed_lot_t(Type, lot) \
        _allocate_zeroed_lot_t<Type>(__FILE__, __LINE__, lot)
    #define free_lot_t(Type, data, lot) \
        _free_lot_t<Type>(__FILE__, __LINE__, data, lot)
    #define construct_t(Type, ...) \
        _construct_t_args<Type>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
    #define deconstruct_t(Type, data) \
        _deconstruct_t<Type>(__FILE__, __LINE__, data)
    #define allocate_raw(size_bytes, alignment) \
        _allocate_raw(__FILE__, __LINE__, size_bytes, alignment)
    #define allocate_zeroed_raw(size_bytes, alignment) \
        _allocate_zeroed_raw(__FILE__, __LINE__, size_bytes, alignment)
    #define free_raw(data, size_bytes) \
        _free_raw(__FILE__, __LINE__, data, size_bytes)
    #define NK_ALLOCATOR_NAME(allocator) \
        allocator->_allocator_name()

#else

    #define allocator_init(AllocatorType, name, type, ...) \
        _allocator_init_untracked<AllocatorType>(__VA_ARGS__)
    #define allocator_init_tracked(AllocatorType, tracker, name, type, ...) \
        _allocator_init_untracked<AllocatorType>(__VA_ARGS__)
    #define allocate_t(Type) \
        _allocate_t<Type>()
    #define allocate_zeroed_t(Type) \
        _allocate_zeroed_t<Type>()
    #define free_t(Type, data) \
        _free_t<Type>(data)
    #define allocate_lot_t(Type, lot) \
        _allocate_lot_t<Type>(lot)
    #define allocate_zeroed_lot_t(Type, lot) \
        _allocate_zeroed_lot_t<Type>(lot)
    #define free_lot_t(Type, data, lot) \
        _free_lot_t<Type>(data, lot)
    #define construct_t(Type, ...) \
        _construct_t<Type>(__VA_ARGS__)
    #define deconstruct_t(Type, data) \
        _deconstruct_t<Type>(data)
    #define allocate_raw(size_bytes, alignment) \
        _allocate_raw(size_bytes, alignment)
    #define allocate_zeroed_raw(size_bytes, alignment) \
        _allocate_zeroed_raw(size_bytes, alignment)
    #define free_raw(data, size_bytes) \
        _free_raw(data, size_bytes)
    #define NK_ALLOCATOR_NAME(allocator) \
        allocator->_allocator_name()

#endif

#define allocator_init_untracked(AllocatorType, ...) \
    _allocator_init_untracked<AllocatorType>(__VA_ARGS__)
