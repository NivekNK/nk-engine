#include "nkpch.h"

#include "memory/linear_allocator.h"

namespace nk::mem {
    namespace {
        void linear_allocator_diagnostic(cstr message) noexcept {
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
        }
    }

    LinearAllocator::LinearAllocator() noexcept
        : Allocator(),
          m_backing_allocator{nullptr},
          m_owns_memory{false} {}

    LinearAllocator::LinearAllocator(
        Untracked,
        Allocator& backing_allocator,
        u64 size_bytes)
        : LinearAllocator() {
        _allocator_init_untracked<LinearAllocator>(backing_allocator, size_bytes);
    }

    LinearAllocator::LinearAllocator(
        Untracked,
        u64 size_bytes,
        void* external_data)
        : LinearAllocator() {
        _allocator_init_untracked<LinearAllocator>(size_bytes, external_data);
    }

    LinearAllocator::~LinearAllocator() {
        if (!_release_backing())
            linear_allocator_diagnostic(
                "nk::mem::LinearAllocator could not release owned backing memory.\n");
    }

    LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
        : Allocator(std::move(other)),
          m_backing_allocator{other.m_backing_allocator},
          m_owns_memory{other.m_owns_memory} {
        other.m_backing_allocator = nullptr;
        other.m_owns_memory = false;
    }

    LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept {
        if (this == &other)
            return *this;

        if (!_can_accept_move()) {
            constexpr cstr message = "nk::mem::LinearAllocator move assignment rejected.\n";
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
            return *this;
        }

        if (!_release_backing()) {
            linear_allocator_diagnostic(
                "nk::mem::LinearAllocator move assignment could not release destination backing memory.\n");
            return *this;
        }

        Allocator::operator=(std::move(other));
        m_backing_allocator = other.m_backing_allocator;
        m_owns_memory = other.m_owns_memory;
        other.m_backing_allocator = nullptr;
        other.m_owns_memory = false;
        return *this;
    }

    bool LinearAllocator::init(
        Allocator& backing_allocator,
        const u64 size_bytes) noexcept {
        if (&backing_allocator == this || size_bytes == 0 ||
            size_bytes > static_cast<u64>(std::numeric_limits<std::size_t>::max())) {
            return false;
        }

        void* data = backing_allocator._allocate_raw(
            size_bytes,
            alignof(std::max_align_t));
        if (data == nullptr)
            return false;

        m_backing_allocator = &backing_allocator;
        m_reserved_bytes = size_bytes;
        m_data = data;
        m_owns_memory = true;
        return true;
    }

    bool LinearAllocator::init(
        const u64 size_bytes,
        void* external_data) noexcept {
        if (size_bytes == 0 || external_data == nullptr ||
            size_bytes > static_cast<u64>(std::numeric_limits<std::size_t>::max())) {
            return false;
        }

        m_backing_allocator = nullptr;
        m_reserved_bytes = size_bytes;
        m_data = external_data;
        m_owns_memory = false;
        return true;
    }

    void* LinearAllocator::_do_allocate(
        const u64 size_bytes,
        const u64 alignment) noexcept {
        if (m_data == nullptr) {
            linear_allocator_diagnostic(
                "nk::mem::LinearAllocator has no backing storage.\n");
            return nullptr;
        }

        constexpr u64 maximum_address = static_cast<u64>(
            std::numeric_limits<std::uintptr_t>::max());
        const u64 base_address = static_cast<u64>(
            reinterpret_cast<std::uintptr_t>(m_data));
        if (alignment > maximum_address || m_used_bytes > maximum_address - base_address) {
            linear_allocator_diagnostic(
                "nk::mem::LinearAllocator cursor address overflow.\n");
            return nullptr;
        }

        const u64 current_address = base_address + m_used_bytes;
        const u64 misalignment = current_address & (alignment - 1);
        const u64 padding = misalignment == 0 ? 0 : alignment - misalignment;

        if (padding > maximum_address - current_address ||
            padding > numeric::u64_max - m_used_bytes) {
            linear_allocator_diagnostic(
                "nk::mem::LinearAllocator padding overflow.\n");
            return nullptr;
        }

        const u64 aligned_offset = m_used_bytes + padding;
        if (size_bytes > numeric::u64_max - aligned_offset) {
            linear_allocator_diagnostic(
                "nk::mem::LinearAllocator allocation offset overflow.\n");
            return nullptr;
        }

        const u64 aligned_address = current_address + padding;
        if (size_bytes > maximum_address - aligned_address) {
            linear_allocator_diagnostic(
                "nk::mem::LinearAllocator allocation address overflow.\n");
            return nullptr;
        }

        const u64 used_bytes = aligned_offset + size_bytes;
        if (used_bytes > m_reserved_bytes ||
            m_active_allocations == numeric::u64_max) {
            linear_allocator_diagnostic(
                "nk::mem::LinearAllocator capacity exceeded.\n");
            return nullptr;
        }

        void* block = static_cast<u8*>(m_data) + aligned_offset;
        ++m_active_allocations;
        m_used_bytes = used_bytes;
        _update_peak();
        return block;
    }

    bool LinearAllocator::_do_free(
        [[maybe_unused]] void* data,
        [[maybe_unused]] u64 size_bytes) noexcept {
        constexpr cstr message =
            "nk::mem::LinearAllocator does not support individual free.\n";
        os::write(message, std::char_traits<char>::length(message));
        os::flush();
        return false;
    }

    bool LinearAllocator::_reset(
        SourceLocation source,
        LinearResetMode mode) noexcept {
        if (!is_initialized() || m_reserved_bytes == 0 || m_data == nullptr)
            return false;

        if (mode == LinearResetMode::ZeroMemory) {
            std::memset(
                m_data,
                0,
                static_cast<std::size_t>(m_reserved_bytes));
        }

        m_active_allocations = 0;
        m_used_bytes = 0;
        _notify_reset(source);

        return true;
    }

    bool LinearAllocator::reset(LinearResetMode mode) noexcept {
        return _reset({nullptr, 0}, mode);
    }

    bool LinearAllocator::_free_linear_allocator() noexcept {
        return reset();
    }

#if NK_MEMORY_TRACKING_ENABLED
    bool LinearAllocator::_free_linear_allocator(cstr file, u32 line) noexcept {
        return _reset({file, line}, default_linear_reset_mode);
    }
#endif

    bool LinearAllocator::_release_backing() noexcept {
        if (!m_owns_memory || m_data == nullptr)
            return true;
        if (m_backing_allocator == nullptr)
            return false;

        if (!m_backing_allocator->_free_raw(m_data, m_reserved_bytes))
            return false;

        m_backing_allocator = nullptr;
        m_owns_memory = false;
        m_reserved_bytes = 0;
        m_data = nullptr;
        return true;
    }
}
