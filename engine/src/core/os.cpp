#include "nkpch.h"

#include "core/os.h"
#include "memory/allocation_tracker.h"

namespace nk::os {
    void* allocate_raw(u64 size_bytes, u64 alignment) noexcept {
        if (size_bytes == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0)
            return nullptr;
        return std::malloc(static_cast<std::size_t>(size_bytes));
    }

    bool free_raw(void* data, [[maybe_unused]] u64 size_bytes) noexcept {
        if (data == nullptr)
            return false;
        std::free(data);
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
        if (!(free_raw)(data, size_bytes))
            return;

        mem::default_allocation_tracker().on_free({
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
