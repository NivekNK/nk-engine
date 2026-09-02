#include "nkpch.h"

#include "memory/linear_allocator.h"

namespace nk::mem {
    LinearAllocator::LinearAllocator() noexcept
        : Allocator(),
          m_owns_memory{false} {}

    LinearAllocator::LinearAllocator(Untracked, u64 size_bytes, void* data)
        : LinearAllocator() {
        _allocator_init_untracked<LinearAllocator>(size_bytes, data);
    }

    LinearAllocator::~LinearAllocator() {
        if (m_owns_memory && m_data != nullptr) {
            std::free(m_data);
        }
    }

    LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
        : Allocator(std::move(other)),
          m_owns_memory{other.m_owns_memory} {
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

        if (m_owns_memory && m_data != nullptr)
            std::free(m_data);

        Allocator::operator=(std::move(other));
        m_owns_memory = other.m_owns_memory;
        other.m_owns_memory = false;
        return *this;
    }

    void LinearAllocator::init(u64 size_bytes, void* data) {
        Assert(size_bytes > 0, "Linear Allocator initialize size_bytes needs to be more than zero.");
        m_reserved_bytes = size_bytes;
        m_owns_memory = data == nullptr;
        if (m_owns_memory) {
            m_data = std::calloc(1, size_bytes);
        } else {
            m_data = data;
        }
    }

    void* LinearAllocator::_do_allocate(
        const u64 size_bytes,
        [[maybe_unused]] const u64 alignment) noexcept {
        if (m_data == nullptr) {
            constexpr cstr message = "nk::mem::LinearAllocator has no backing storage.\n";
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
            return nullptr;
        }

        const u64 used_bytes = m_used_bytes + size_bytes;
        if (used_bytes < m_used_bytes || used_bytes > m_reserved_bytes) {
            constexpr cstr message = "nk::mem::LinearAllocator capacity exceeded.\n";
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
            return nullptr;
        }

        void* block = static_cast<u8*>(m_data) + m_used_bytes;
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

    bool LinearAllocator::_reset(SourceLocation source) noexcept {
        if (!is_initialized() || m_reserved_bytes == 0 || m_data == nullptr)
            return false;

        m_active_allocations = 0;
        m_used_bytes = 0;
        std::memset(m_data, 0, m_reserved_bytes);
        _notify_reset(source);

        return true;
    }

    bool LinearAllocator::_free_linear_allocator() {
        return _reset({nullptr, 0});
    }

#if NK_MEMORY_TRACKING_ENABLED
    bool LinearAllocator::_free_linear_allocator(cstr file, u32 line) {
        return _reset({file, line});
    }
#endif
}
