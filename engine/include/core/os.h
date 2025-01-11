#pragma once

namespace nk::os {
    inline size_t get_thread_id() {
#if defined(NK_PLATFORM_WINDOWS)
        return static_cast<size_t>(::GetCurrentThreadId());
#elif defined(NK_PLATFORM_LINUX)
    #if defined(__ANDROID__) && defined(__ANDROID_API__) && (__ANDROID_API__ < 21)
        #define SYS_gettid __NR_gettid
    #endif
        return static_cast<size_t>(::syscall(SYS_gettid));
#else
        // APPLE
        // uint64_t tid;
        // // There is no pthread_threadid_np prior to Mac OS X 10.6, and it is not supported on any PPC,
        // // including 10.6.8 Rosetta. __POWERPC__ is Apple-specific define encompassing ppc and ppc64.
        // #ifdef MAC_OS_X_VERSION_MAX_ALLOWED
        // {
        //     #if (MAC_OS_X_VERSION_MAX_ALLOWED < 1060) || defined(__POWERPC__)
        //     tid = pthread_mach_thread_np(pthread_self());
        //     #elif MAC_OS_X_VERSION_MIN_REQUIRED < 1060
        //     if (&pthread_threadid_np) {
        //         pthread_threadid_np(nullptr, &tid);
        //     } else {
        //         tid = pthread_mach_thread_np(pthread_self());
        //     }
        //     #else
        //     pthread_threadid_np(nullptr, &tid);
        //     #endif
        // }
        // #else
        // pthread_threadid_np(nullptr, &tid);
        // #endif
        // return static_cast<size_t>(tid);
        return static_cast<size_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
#endif
    }

    inline void write(cstr message, size_t length) {
#if defined(NK_PLATFORM_WINDOWS)
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdout == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD bytes_written;
        ::WriteFile(output, message, static_cast<DWORD>(length), &bytes_written, NULL);
#elif defined(NK_PLATFORM_LINUX)
        ::fwrite(message, sizeof(char), length, stdout);
#else
    #error Not implemented!
#endif
    }

    inline void flush() {
#if defined(NK_PLATFORM_WINDOWS)
#elif defined(NK_PLATFORM_LINUX)
        ::fflush(stdout);
#else
    #error Not implemented!
#endif
    }

    void* native_allocate_impl(u64 size_bytes, u64 alignment);

    void native_free_impl(void* data, u64 size_bytes);

    template <typename T>
    T* native_allocate_lot_impl(u64 lot) {
        return static_cast<T*>(native_allocate_impl(sizeof(T) * lot, alignof(T)));
    }

    template <typename T>
    void native_free_lot_impl(T* data, u64 lot) {
        native_free_impl(data, sizeof(T) * lot);
    }

    template <typename T, typename... Args>
    inline T* native_construct_impl(Args&&... args) {
        return new (os::native_allocate_impl(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
    }

    template <typename T, typename V>
    inline void native_deconstruct_impl(V* data) {
        if (data == nullptr)
            return;

        data->~V();
        os::native_free_impl(data, sizeof(T));
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    void* _native_allocate_impl(cstr file, u32 line, u64 size_bytes, u64 alignment);

    void _native_free_impl(cstr file, u32 line, void* data, u64 size_bytes);

    template <typename T>
    T* _native_allocate_lot_impl(cstr file, u32 line, u64 lot) {
        return static_cast<T*>(_native_allocate_impl(file, line, sizeof(T) * lot, alignof(T)));
    }

    template <typename T>
    void _native_free_lot_impl(cstr file, u32 line, T* data, u64 lot) {
        _native_free_impl(file, line, data, sizeof(T) * lot);
    }

    template <typename T, typename... Args>
    inline T* _native_construct_impl(cstr file, u32 line, Args&&... args) {
        return new (os::_native_allocate_impl(file, line, sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
    }

    template <typename T, typename V>
    inline void _native_deconstruct_impl(cstr file, u32 line, V* data) {
        if (data == nullptr)
            return;

        data->~V();
        os::_native_free_impl(file, line, data, sizeof(T));
    }

#endif
}

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #define native_allocate(size_bytes, alignment) \
        nk::os::_native_allocate_impl(__FILE__, __LINE__, size_bytes, alignment)
    #define native_free(data, size_bytes) \
        nk::os::_native_free_impl(__FILE__, __LINE__, data, size_bytes)
    #define native_allocate_lot(Type, lot) \
        nk::os::_native_allocate_lot_impl<Type>(__FILE__, __LINE__, lot)
    #define native_free_lot(Type, data, lot) \
        nk::os::_native_free_lot_impl<Type>(__FILE__, __LINE__, data, lot)
    #define native_construct(Type, ...) \
        nk::os::_native_construct_impl<Type>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
    #define native_deconstruct(Type, data) \
        nk::os::_native_deconstruct_impl<Type>(__FILE__, __LINE__, data)

#else

    #define native_allocate(size_bytes, alignment) \
        nk::os::native_allocate_impl(size_bytes, alignment)
    #define native_free(data, size_bytes) \
        nk::os::native_free_impl(data, size_bytes)
    #define native_allocate_lot(Type, lot) \
        nk::os::native_allocate_lot_impl<Type>(lot)
    #define native_free_lot(Type, data, lot) \
        nk::os::native_free_lot_impl<Type>(data, lot)
    #define native_construct(Type, ...) \
        nk::os::native_construct_impl<Type>(__VA_ARGS__)
    #define native_deconstruct(Type, data) \
        nk::os::native_deconstruct_impl<Type>(data)

#endif
