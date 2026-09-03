#pragma once

#include "core/result.h"
#include "memory/free_list.h"
#include "renderer/renderer_result.h"

namespace nk {
    /**
     * Tracks occupied byte ranges in a renderer buffer without owning the
     * buffer or its bytes.
     */
    class BufferSuballocator final {
    public:
        BufferSuballocator() noexcept = default;
        ~BufferSuballocator() = default;

        BufferSuballocator(const BufferSuballocator&) = delete;
        BufferSuballocator& operator=(const BufferSuballocator&) = delete;
        BufferSuballocator(BufferSuballocator&&) noexcept = default;
        BufferSuballocator& operator=(BufferSuballocator&&) noexcept = default;

        [[nodiscard]] result<void, renderer_error> init(
            mem::Allocator& metadata_allocator,
            u64 total_size,
            u64 metadata_capacity) noexcept;
        void shutdown() noexcept;

        [[nodiscard]] result<mem::MemoryRange, renderer_error> reserve(
            u64 size,
            u64 alignment = 1) noexcept;
        [[nodiscard]] result<void, renderer_error> release(
            mem::MemoryRange range) noexcept;
        [[nodiscard]] result<void, renderer_error> resize(
            u64 new_size) noexcept;

        [[nodiscard]] bool initialized() const noexcept {
            return m_ranges.initialized();
        }
        [[nodiscard]] u64 size() const noexcept {
            return m_ranges.total_size();
        }
        [[nodiscard]] u64 free_space() const noexcept {
            return m_ranges.free_space();
        }
        [[nodiscard]] u64 occupied_space() const noexcept {
            return m_ranges.used_space();
        }
        [[nodiscard]] u64 metadata_capacity() const noexcept {
            return m_ranges.metadata_capacity();
        }

    private:
        static renderer_error translate_error(
            renderer_error_code operation,
            mem::free_list_error error) noexcept;

        mem::FreeList m_ranges;
    };
}
