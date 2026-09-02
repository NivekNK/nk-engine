#include "nkpch.h"

#include "memory/malloc_allocator.h"

namespace nk::mem {
    MallocAllocator::MallocAllocator() noexcept
        : Allocator() {}

    MallocAllocator::MallocAllocator(Untracked)
        : MallocAllocator() {
        _allocator_init_untracked<MallocAllocator>();
    }

    MallocAllocator::~MallocAllocator() = default;

    MallocAllocator::MallocAllocator(MallocAllocator&& other) noexcept
        : Allocator(std::move(other)) {}

    MallocAllocator& MallocAllocator::operator=(MallocAllocator&& other) noexcept {
        Allocator::operator=(std::move(other));
        return *this;
    }

    void MallocAllocator::init() {}

    void* MallocAllocator::_do_allocate(
        const u64 size_bytes,
        const u64 alignment) noexcept {
        if (size_bytes > numeric::u64_max - m_reserved_bytes ||
            size_bytes > numeric::u64_max - m_used_bytes ||
            m_active_allocations == numeric::u64_max) {
            constexpr cstr message =
                "nk::mem::MallocAllocator rejected allocation statistics overflow.\n";
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
            return nullptr;
        }

        void* data = (os::allocate_raw)(size_bytes, alignment);
        if (data == nullptr)
            return nullptr;

        ++m_active_allocations;
        m_reserved_bytes += size_bytes;
        m_used_bytes += size_bytes;
        _update_peak();
        return data;
    }

    bool MallocAllocator::_do_free(void* data, const u64 size_bytes) noexcept {
        if (m_active_allocations == 0 || size_bytes > m_used_bytes ||
            size_bytes > m_reserved_bytes) {
            constexpr cstr message = "nk::mem::MallocAllocator rejected invalid free.\n";
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
            return false;
        }

        if (!(os::free_raw)(data, size_bytes))
            return false;

        --m_active_allocations;
        m_reserved_bytes -= size_bytes;
        m_used_bytes -= size_bytes;
        return true;
    }
}
