#include "nkpch.h"

#include "renderer/buffer_suballocator.h"

namespace nk {
    result<void, renderer_error> BufferSuballocator::init(
        mem::Allocator& metadata_allocator,
        const u64 total_size,
        const u64 metadata_capacity) noexcept {
        auto initialized = m_ranges.init(
            metadata_allocator,
            total_size,
            metadata_capacity);
        if (!initialized) {
            return err(translate_error(
                renderer_error_code::buffer_allocation_failed,
                initialized.error()));
        }
        return ok();
    }

    void BufferSuballocator::shutdown() noexcept {
        m_ranges.shutdown();
    }

    result<mem::MemoryRange, renderer_error> BufferSuballocator::reserve(
        const u64 size,
        const u64 alignment) noexcept {
        auto reserved = m_ranges.reserve(size, alignment);
        if (!reserved) {
            return err(translate_error(
                renderer_error_code::buffer_allocation_failed,
                reserved.error()));
        }
        return ok(*reserved);
    }

    result<void, renderer_error> BufferSuballocator::release(
        const mem::MemoryRange range) noexcept {
        auto released = m_ranges.release(range);
        if (!released) {
            return err(translate_error(
                renderer_error_code::buffer_release_failed,
                released.error()));
        }
        return ok();
    }

    result<void, renderer_error> BufferSuballocator::resize(
        const u64 new_size) noexcept {
        auto resized = m_ranges.resize(new_size);
        if (!resized) {
            return err(translate_error(
                renderer_error_code::buffer_resize_failed,
                resized.error()));
        }
        return ok();
    }

    renderer_error BufferSuballocator::translate_error(
        const renderer_error_code operation,
        const mem::free_list_error error) noexcept {
        return {
            .code = operation,
            .native_code = static_cast<i32>(error),
        };
    }
}
