#include "nkpch.h"

#include "memory/free_list_allocator.h"

namespace nk::mem {
    namespace {
        void free_list_allocator_diagnostic(cstr message) noexcept {
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
        }
    }

    FreeListAllocator::FreeListAllocator() noexcept
        : Allocator(),
          m_backing_allocator{nullptr},
          m_metadata_allocator{nullptr},
          m_owns_memory{false} {}

    FreeListAllocator::FreeListAllocator(
        Untracked,
        Allocator& backing_allocator,
        Allocator& metadata_allocator,
        const u64 size_bytes,
        const u64 metadata_capacity) noexcept
        : FreeListAllocator() {
        _allocator_init_untracked<FreeListAllocator>(
            backing_allocator,
            metadata_allocator,
            size_bytes,
            metadata_capacity);
    }

    FreeListAllocator::FreeListAllocator(
        Untracked,
        Allocator& metadata_allocator,
        void* external_data,
        const u64 size_bytes,
        const u64 metadata_capacity) noexcept
        : FreeListAllocator() {
        _allocator_init_untracked<FreeListAllocator>(
            metadata_allocator,
            external_data,
            size_bytes,
            metadata_capacity);
    }

    FreeListAllocator::~FreeListAllocator() {
        if (!release_resources()) {
            free_list_allocator_diagnostic(
                "nk::mem::FreeListAllocator could not release its backing storage.\n");
        }
    }

    FreeListAllocator::FreeListAllocator(FreeListAllocator&& other) noexcept
        : Allocator(std::move(other)),
          m_free_list{std::move(other.m_free_list)},
          m_backing_allocator{other.m_backing_allocator},
          m_metadata_allocator{other.m_metadata_allocator},
          m_owns_memory{other.m_owns_memory} {
        other.m_backing_allocator = nullptr;
        other.m_metadata_allocator = nullptr;
        other.m_owns_memory = false;
    }

    FreeListAllocator& FreeListAllocator::operator=(
        FreeListAllocator&& other) noexcept {
        if (this == &other)
            return *this;

        if (!_can_accept_move()) {
            free_list_allocator_diagnostic(
                "nk::mem::FreeListAllocator move assignment rejected.\n");
            return *this;
        }

        if (!release_resources()) {
            free_list_allocator_diagnostic(
                "nk::mem::FreeListAllocator move assignment could not release destination storage.\n");
            return *this;
        }

        Allocator::operator=(std::move(other));
        m_free_list = std::move(other.m_free_list);
        move_from(other);
        return *this;
    }

    bool FreeListAllocator::init(
        Allocator& backing_allocator,
        Allocator& metadata_allocator,
        const u64 size_bytes,
        const u64 metadata_capacity) noexcept {
        if (&backing_allocator == this ||
            !valid_configuration(
                metadata_allocator,
                size_bytes,
                metadata_capacity)) {
            return false;
        }

        void* data = backing_allocator._allocate_raw(
            size_bytes,
            alignof(std::max_align_t));
        if (data == nullptr)
            return false;

        auto initialized = m_free_list.init(
            metadata_allocator,
            size_bytes,
            metadata_capacity);
        if (!initialized) {
            if (!backing_allocator._free_raw(data, size_bytes)) {
                free_list_allocator_diagnostic(
                    "nk::mem::FreeListAllocator could not roll back its backing allocation.\n");
            }
            return false;
        }

        m_backing_allocator = &backing_allocator;
        m_metadata_allocator = &metadata_allocator;
        m_owns_memory = true;
        m_reserved_bytes = size_bytes;
        m_data = data;
        return true;
    }

    bool FreeListAllocator::init(
        Allocator& metadata_allocator,
        void* external_data,
        const u64 size_bytes,
        const u64 metadata_capacity) noexcept {
        if (!valid_configuration(
                metadata_allocator,
                size_bytes,
                metadata_capacity) ||
            !valid_address_range(external_data, size_bytes)) {
            return false;
        }

        auto initialized = m_free_list.init(
            metadata_allocator,
            size_bytes,
            metadata_capacity);
        if (!initialized)
            return false;

        m_backing_allocator = nullptr;
        m_metadata_allocator = &metadata_allocator;
        m_owns_memory = false;
        m_reserved_bytes = size_bytes;
        m_data = external_data;
        return true;
    }

    void* FreeListAllocator::_do_allocate(
        const u64 size_bytes,
        const u64 alignment) noexcept {
        if (m_data == nullptr ||
            size_bytes > numeric::u64_max - allocation_overhead ||
            size_bytes > numeric::u64_max - m_used_bytes ||
            m_active_allocations == numeric::u64_max) {
            return nullptr;
        }

        const u64 range_size = allocation_overhead + size_bytes;
        const u64 alignment_mask = alignment - 1;
        const u64 base_address = static_cast<u64>(
            reinterpret_cast<std::uintptr_t>(m_data));
        const u64 alignment_bias =
            ((base_address & alignment_mask) +
             (allocation_overhead & alignment_mask)) &
            alignment_mask;

        auto reserved = m_free_list.reserve(
            range_size,
            alignment,
            alignment_bias);
        if (!reserved)
            return nullptr;

        const AllocationHeader header{
            .guard = header_guard(
                base_address,
                reserved->offset,
                size_bytes),
        };
        u8* header_data = static_cast<u8*>(m_data) + reserved->offset;
        std::memcpy(header_data, &header, sizeof(header));

        ++m_active_allocations;
        m_used_bytes += size_bytes;
        _update_peak();
        return header_data + allocation_overhead;
    }

    bool FreeListAllocator::_do_free(
        void* data,
        const u64 size_bytes) noexcept {
        if (m_data == nullptr || data == nullptr || size_bytes == 0 ||
            m_active_allocations == 0 || size_bytes > m_used_bytes) {
            return false;
        }

        constexpr u64 maximum_address = static_cast<u64>(
            std::numeric_limits<std::uintptr_t>::max());
        const u64 base_address = static_cast<u64>(
            reinterpret_cast<std::uintptr_t>(m_data));
        const u64 data_address = static_cast<u64>(
            reinterpret_cast<std::uintptr_t>(data));
        if (m_reserved_bytes > maximum_address - base_address)
            return false;

        const u64 end_address = base_address + m_reserved_bytes;
        if (data_address < base_address || data_address >= end_address)
            return false;

        const u64 payload_offset = data_address - base_address;
        if (payload_offset < allocation_overhead ||
            size_bytes > m_reserved_bytes - payload_offset) {
            return false;
        }

        const u64 header_offset = payload_offset - allocation_overhead;
        u8* header_data = static_cast<u8*>(m_data) + header_offset;
        AllocationHeader header{};
        std::memcpy(&header, header_data, sizeof(header));
        if (header.guard != header_guard(
                base_address,
                header_offset,
                size_bytes)) {
            return false;
        }

        auto released = m_free_list.release({
            .offset = header_offset,
            .size = allocation_overhead + size_bytes,
        });
        if (!released)
            return false;

        std::memset(header_data, 0, sizeof(header));
        --m_active_allocations;
        m_used_bytes -= size_bytes;
        return true;
    }

    bool FreeListAllocator::valid_configuration(
        Allocator& metadata_allocator,
        const u64 size_bytes,
        const u64 metadata_capacity) const noexcept {
        return &metadata_allocator != this &&
               size_bytes > allocation_overhead &&
               metadata_capacity != 0 &&
               size_bytes <= static_cast<u64>(
                   std::numeric_limits<std::size_t>::max());
    }

    bool FreeListAllocator::valid_address_range(
        void* data,
        const u64 size_bytes) noexcept {
        if (data == nullptr)
            return false;
        constexpr u64 maximum_address = static_cast<u64>(
            std::numeric_limits<std::uintptr_t>::max());
        const u64 base_address = static_cast<u64>(
            reinterpret_cast<std::uintptr_t>(data));
        return size_bytes <= maximum_address - base_address;
    }

    bool FreeListAllocator::release_resources() noexcept {
        if (m_owns_memory && m_data != nullptr) {
            if (m_backing_allocator == nullptr ||
                !m_backing_allocator->_free_raw(m_data, m_reserved_bytes)) {
                return false;
            }
        }

        m_free_list.shutdown();
        m_backing_allocator = nullptr;
        m_metadata_allocator = nullptr;
        m_owns_memory = false;
        m_reserved_bytes = 0;
        m_data = nullptr;
        return true;
    }

    u64 FreeListAllocator::header_guard(
        const u64 base_address,
        const u64 offset,
        const u64 size_bytes) noexcept {
        return allocation_magic ^ base_address ^ offset ^ size_bytes;
    }

    void FreeListAllocator::move_from(
        FreeListAllocator& other) noexcept {
        m_backing_allocator = other.m_backing_allocator;
        m_metadata_allocator = other.m_metadata_allocator;
        m_owns_memory = other.m_owns_memory;

        other.m_backing_allocator = nullptr;
        other.m_metadata_allocator = nullptr;
        other.m_owns_memory = false;
    }
}
