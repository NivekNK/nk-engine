#pragma once

namespace nk::os {
    inline std::size_t get_thread_id() {
#if defined(NK_PLATFORM_WINDOWS)
        return static_cast<std::size_t>(::GetCurrentThreadId());
#elif defined(NK_PLATFORM_LINUX)
    #if defined(__ANDROID__) && defined(__ANDROID_API__) && (__ANDROID_API__ < 21)
        #define SYS_gettid __NR_gettid
    #endif
        return static_cast<std::size_t>(::syscall(SYS_gettid));
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
        // return static_cast<std::size_t>(tid);
        return static_cast<std::size_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
#endif
    }

    inline void write(cstr message, std::size_t length) {
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

    inline void debug_break() {
#if defined(NK_PLATFORM_WINDOWS)
        __debugbreak();
#elif defined(NK_PLATFORM_LINUX)
        __builtin_trap();
#else
    #error Not implemented!
#endif
    }

    void* _native_allocate(u64 size_bytes, u64 alignment);

    void _native_free(void* data, u64 size_bytes);

    template <typename T>
    T* _native_allocate_lot(u64 lot) {
        return static_cast<T*>(_native_allocate(sizeof(T) * lot, alignof(T)));
    }

    template <typename T>
    void _native_free_lot(T* data, u64 lot) {
        _native_free(data, sizeof(T) * lot);
    }

    template <typename T, typename... Args>
    inline T* _native_construct(Args&&... args) {
        return new (_native_allocate(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
    }

    template <typename T, typename V>
    inline void _native_deconstruct(V* data) {
        if (data == nullptr)
            return;

        data->~V();
        _native_free(data, sizeof(T));
    }

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    void* _native_allocate(cstr file, u32 line, u64 size_bytes, u64 alignment);

    void _native_free(cstr file, u32 line, void* data, u64 size_bytes);

    template <typename T>
    T* _native_allocate_lot(cstr file, u32 line, u64 lot) {
        return static_cast<T*>(_native_allocate(file, line, sizeof(T) * lot, alignof(T)));
    }

    template <typename T>
    void _native_free_lot(cstr file, u32 line, T* data, u64 lot) {
        _native_free(file, line, data, sizeof(T) * lot);
    }

    template <typename T, typename... Args>
    inline T* _native_construct_args(cstr file, u32 line, Args&&... args) {
        return new (_native_allocate(file, line, sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
    }

    template <typename T, typename V>
    inline void _native_deconstruct(cstr file, u32 line, V* data) {
        if (data == nullptr)
            return;

        data->~V();
        _native_free(file, line, data, sizeof(T));
    }

#endif
}

#if NK_DEV_MODE <= NK_RELEASE_DEBUG_INFO && NK_ACTIVE_MEMORY_SYSTEM

    #define native_allocate(size_bytes, alignment) \
        nk::os::_native_allocate(__FILE__, __LINE__, size_bytes, alignment)
    #define native_free(data, size_bytes) \
        nk::os::_native_free(__FILE__, __LINE__, data, size_bytes)
    #define native_allocate_lot(Type, lot) \
        nk::os::_native_allocate_lot<Type>(__FILE__, __LINE__, lot)
    #define native_free_lot(Type, data, lot) \
        nk::os::_native_free_lot<Type>(__FILE__, __LINE__, data, lot)
    #define native_construct(Type, ...) \
        nk::os::_native_construct_args<Type>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
    #define native_deconstruct(Type, data) \
        nk::os::_native_deconstruct<Type>(__FILE__, __LINE__, data)

#else

    #define native_allocate(size_bytes, alignment) \
        nk::os::_native_allocate(size_bytes, alignment)
    #define native_free(data, size_bytes) \
        nk::os::_native_free(data, size_bytes)
    #define native_allocate_lot(Type, lot) \
        nk::os::_native_allocate_lot<Type>(lot)
    #define native_free_lot(Type, data, lot) \
        nk::os::_native_free_lot<Type>(data, lot)
    #define native_construct(Type, ...) \
        nk::os::_native_construct<Type>(__VA_ARGS__)
    #define native_deconstruct(Type, data) \
        nk::os::_native_deconstruct<Type>(data)

#endif
