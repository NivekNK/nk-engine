#include "nkpch.h"

#include "core/os.h"
#include "memory/allocation_tracker.h"

#if defined(NK_PLATFORM_WINDOWS)
    #include <malloc.h>
#endif

namespace nk::os {
    void* allocate_raw(u64 size_bytes, u64 alignment) noexcept {
        // posix_memalign requires a power-of-two multiple of sizeof(void*),
        // and _aligned_malloc accepts the same normalized value.
        constexpr u64 platform_minimum_alignment = sizeof(void*);
        constexpr u64 maximum_size =
            static_cast<u64>(std::numeric_limits<std::size_t>::max());

        if (size_bytes == 0 || size_bytes > maximum_size || alignment == 0 ||
            alignment > maximum_size || (alignment & (alignment - 1)) != 0) {
            return nullptr;
        }

        const std::size_t effective_alignment = static_cast<std::size_t>(
            alignment < platform_minimum_alignment
                ? platform_minimum_alignment
                : alignment);

#if defined(NK_PLATFORM_WINDOWS)
        return ::_aligned_malloc(static_cast<std::size_t>(size_bytes), effective_alignment);
#elif defined(NK_PLATFORM_LINUX)
        void* data = nullptr;
        if (::posix_memalign(
                &data,
                effective_alignment,
                static_cast<std::size_t>(size_bytes)) != 0) {
            return nullptr;
        }
        return data;
#else
    #error Not implemented!
#endif
    }

    bool free_raw(void* data, u64 size_bytes) noexcept {
        if (data == nullptr || size_bytes == 0)
            return false;

#if defined(NK_PLATFORM_WINDOWS)
        ::_aligned_free(data);
#elif defined(NK_PLATFORM_LINUX)
        std::free(data);
#else
    #error Not implemented!
#endif
        return true;
    }

    void* _native_allocate(u64 size_bytes, u64 alignment) noexcept {
        return (allocate_raw)(size_bytes, alignment);
    }

    void _native_free(void* data, u64 size_bytes) noexcept {
        (free_raw)(data, size_bytes);
    }

#if NK_MEMORY_TRACKING_ENABLED

    void* _native_allocate(
        cstr file,
        u32 line,
        u64 size_bytes,
        u64 alignment) noexcept {
        void* data = (allocate_raw)(size_bytes, alignment);
        if (data == nullptr)
            return nullptr;

        mem::default_allocation_tracker().on_allocate({
            .allocator_id = mem::native_allocator_id,
            .address = data,
            .size_bytes = size_bytes,
            .alignment = alignment,
            .source = {file, line},
            .statistics = {},
        });
        return data;
    }

    void _native_free(cstr file, u32 line, void* data, u64 size_bytes) noexcept {
        if (data == nullptr || size_bytes == 0)
            return;

        mem::AllocationTracker& tracker = mem::default_allocation_tracker();
        if (tracker.validate_free(mem::native_allocator_id, data, size_bytes) !=
            mem::FreeValidation::Valid) {
            constexpr cstr message = "nk::os native free rejected by allocation tracker.\n";
            write(message, std::char_traits<char>::length(message));
            flush();
            return;
        }

        if (!(free_raw)(data, size_bytes))
            return;

        tracker.on_free({
            .allocator_id = mem::native_allocator_id,
            .address = data,
            .size_bytes = size_bytes,
            .alignment = 0,
            .source = {file, line},
            .statistics = {},
        });
    }

#endif
}
