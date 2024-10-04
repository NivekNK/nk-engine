#pragma once

#if !defined(NK_RELEASE)

namespace nk::mem {
    void* allocate_impl(cstr file, const u64 line, const u64 size_bytes);

    void free_impl(cstr file, const u64 line, const u64 size_bytes, void* ptr);

    template <typename T, typename ...Args>
    T* construct_impl(cstr file, const u64 line, Args&&... args) {
        return new (allocate_impl(file, line, sizeof(T))) T(std::forward<Args>(args)...);
    }

    template <typename T, typename V>
    void destroy_impl(cstr file, const u64 line, V* ptr) {
        if (ptr == nullptr) {
            return;
        }

        ptr->~V();
        free_impl(file, line, ptr, sizeof(T));
    }
}

#define allocate(size_bytes) allocate_impl(__FILE__, __LINE__, size_bytes)
#define free(size_bytes, ptr) free_impl(__FILE__, __LINE__, size_bytes, ptr)
#define construct(T, ...) contruct_impl<T>(__FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define destroy(T, ptr) destroy_impl<T>(__FILE__, __LINE__, ptr)

#else

namespace nk::mem {
    void* allocate_impl(const u64 size_bytes);

    void free_impl(void* ptr);

    template <typename T, typename ...Args>
    T* construct_impl(Args&&... args) {
        return new (allocate_impl(sizeof(T))) T(std::forward<Args>(args)...);
    }

    template <typename T>
    void destroy_impl(T* ptr) {
        if (ptr == nullptr) {
            return;
        }

        ptr->~T();
        free_impl(ptr);
    }
}

#define allocate(size_bytes) allocate_impl(size_bytes)
#define free(size_bytes, ptr) free_impl(ptr)
#define construct(T, ...) contruct_impl<T>(__VA_ARGS__)
#define destroy(T, ptr) destroy_impl<T>(ptr)

#endif
