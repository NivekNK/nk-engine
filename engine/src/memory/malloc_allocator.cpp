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
        [[maybe_unused]] const u64 alignment) noexcept {
        void* data = std::calloc(1, static_cast<std::size_t>(size_bytes));
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

        std::free(data);
        --m_active_allocations;
        m_reserved_bytes -= size_bytes;
        m_used_bytes -= size_bytes;
        return true;
    }
}
