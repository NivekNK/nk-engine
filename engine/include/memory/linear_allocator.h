#pragma once

#include "memory/allocator.h"

namespace nk::mem {
    enum class LinearResetMode : u8 {
        RetainContents,
        ZeroMemory,
    };

#if NK_MEMORY_TRACKING_ENABLED
    inline constexpr LinearResetMode default_linear_reset_mode =
        LinearResetMode::ZeroMemory;
#else
    inline constexpr LinearResetMode default_linear_reset_mode =
        LinearResetMode::RetainContents;
#endif

    class LinearAllocator : public Allocator {
    public:
        LinearAllocator() noexcept;
        LinearAllocator(Untracked, Allocator& backing_allocator, u64 size_bytes);
        LinearAllocator(Untracked, u64 size_bytes, void* external_data);
        ~LinearAllocator() override;

        LinearAllocator(LinearAllocator&& other) noexcept;
        LinearAllocator& operator=(LinearAllocator&& other) noexcept;

        LinearAllocator(LinearAllocator&) = delete;
        LinearAllocator& operator=(LinearAllocator&) = delete;

        bool init(Allocator& backing_allocator, u64 size_bytes) noexcept;
        bool init(u64 size_bytes, void* external_data) noexcept;

        bool reset(LinearResetMode mode = default_linear_reset_mode) noexcept;

        cstr to_cstr() const noexcept override { return "LinearAllocator"; }
        bool owns_backing_memory() const noexcept { return m_owns_memory; }
        Allocator* backing_allocator() const noexcept { return m_backing_allocator; }

    protected:
        void* _do_allocate(u64 size_bytes, u64 alignment) noexcept override;
        bool _do_free(void* data, u64 size_bytes) noexcept override;

    private:
        bool _reset(SourceLocation source, LinearResetMode mode) noexcept;
        bool _release_backing() noexcept;
        Allocator* m_backing_allocator;
        bool m_owns_memory;
    };
}
