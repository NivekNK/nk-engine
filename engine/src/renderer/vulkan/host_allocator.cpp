#include "nkpch.h"

#include "vulkan/host_allocator.h"

namespace nk::vk {
    namespace {
        constexpr u64 allocation_magic = 0x4e4b564b484f5354ull;

        struct alignas(std::max_align_t) AllocationHeader {
            void* base = nullptr;
            u64 raw_size = 0;
            u64 requested_size = 0;
            u64 magic = 0;
        };

        [[nodiscard]] bool valid_alignment(const std::size_t alignment) noexcept {
            return alignment != 0 && (alignment & (alignment - 1)) == 0;
        }

        [[nodiscard]] std::size_t normalized_alignment(
            const std::size_t requested) noexcept {
            return requested < alignof(AllocationHeader)
                ? alignof(AllocationHeader)
                : requested;
        }

        [[nodiscard]] AllocationHeader* header_for(void* allocation) noexcept {
            return reinterpret_cast<AllocationHeader*>(
                static_cast<u8*>(allocation) - sizeof(AllocationHeader));
        }
    }

    VulkanHostAllocator::VulkanHostAllocator() noexcept {
        (void)m_mutex.init();
    }

    VulkanHostAllocator::~VulkanHostAllocator() {
        (void)shutdown();
    }

    bool VulkanHostAllocator::init(mem::Allocator& allocator) noexcept {
        if (m_allocator != nullptr || !allocator.is_initialized() ||
            !m_mutex.initialized()) {
            return false;
        }
        m_allocator = &allocator;
        m_statistics = {};
        m_callbacks = {
            .pUserData = this,
            .pfnAllocation = &VulkanHostAllocator::allocate,
            .pfnReallocation = &VulkanHostAllocator::reallocate,
            .pfnFree = &VulkanHostAllocator::release,
            .pfnInternalAllocation = &VulkanHostAllocator::internal_allocate,
            .pfnInternalFree = &VulkanHostAllocator::internal_release,
        };
        return true;
    }

    bool VulkanHostAllocator::shutdown() noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        if (!lock)
            return false;
        if (m_statistics.active_allocations != 0 ||
            m_statistics.internal_allocations != 0) {
            return false;
        }
        m_callbacks = {};
        m_allocator = nullptr;
        return true;
    }

    VulkanHostAllocatorStatistics VulkanHostAllocator::statistics() const noexcept {
        auto lock = LockGuard::acquire(m_mutex);
        return lock ? m_statistics : VulkanHostAllocatorStatistics{};
    }

    void* VulkanHostAllocator::allocate(
        void* user_data,
        const std::size_t size,
        const std::size_t alignment,
        VkSystemAllocationScope) {
        return user_data == nullptr
            ? nullptr
            : static_cast<VulkanHostAllocator*>(user_data)->allocate_impl(
                  size,
                  alignment);
    }

    void* VulkanHostAllocator::reallocate(
        void* user_data,
        void* original,
        const std::size_t size,
        const std::size_t alignment,
        VkSystemAllocationScope) {
        return user_data == nullptr
            ? nullptr
            : static_cast<VulkanHostAllocator*>(user_data)->reallocate_impl(
                  original,
                  size,
                  alignment);
    }

    void VulkanHostAllocator::release(
        void* user_data,
        void* allocation) {
        if (user_data != nullptr) {
            static_cast<VulkanHostAllocator*>(user_data)->release_impl(
                allocation);
        }
    }

    void VulkanHostAllocator::internal_allocate(
        void* user_data,
        const std::size_t size,
        VkInternalAllocationType,
        VkSystemAllocationScope) {
        if (user_data == nullptr)
            return;
        auto& allocator = *static_cast<VulkanHostAllocator*>(user_data);
        auto lock = LockGuard::acquire(allocator.m_mutex);
        if (!lock ||
            size > numeric::u64_max - allocator.m_statistics.internal_bytes ||
            allocator.m_statistics.internal_allocations == numeric::u64_max) {
            return;
        }
        ++allocator.m_statistics.internal_allocations;
        allocator.m_statistics.internal_bytes += size;
    }

    void VulkanHostAllocator::internal_release(
        void* user_data,
        const std::size_t size,
        VkInternalAllocationType,
        VkSystemAllocationScope) {
        if (user_data == nullptr)
            return;
        auto& allocator = *static_cast<VulkanHostAllocator*>(user_data);
        auto lock = LockGuard::acquire(allocator.m_mutex);
        if (!lock)
            return;
        if (allocator.m_statistics.internal_allocations != 0)
            --allocator.m_statistics.internal_allocations;
        allocator.m_statistics.internal_bytes =
            size > allocator.m_statistics.internal_bytes
                ? 0
                : allocator.m_statistics.internal_bytes - size;
    }

    void* VulkanHostAllocator::allocate_impl(
        const std::size_t size,
        const std::size_t alignment) noexcept {
        if (m_allocator == nullptr || size == 0 ||
            !valid_alignment(alignment)) {
            return nullptr;
        }
        const std::size_t effective_alignment =
            normalized_alignment(alignment);
        constexpr std::size_t header_size = sizeof(AllocationHeader);
        const std::size_t extra = effective_alignment - 1;
        if (size > std::numeric_limits<std::size_t>::max() - header_size ||
            size + header_size >
                std::numeric_limits<std::size_t>::max() - extra) {
            return nullptr;
        }
        const std::size_t raw_size = size + header_size + extra;

        auto lock = LockGuard::acquire(m_mutex);
        if (!lock ||
            size > numeric::u64_max - m_statistics.allocated_bytes ||
            m_statistics.active_allocations == numeric::u64_max) {
            return nullptr;
        }
        void* base = m_allocator->_allocate_raw(
            static_cast<u64>(raw_size),
            alignof(AllocationHeader));
        if (base == nullptr)
            return nullptr;

        const std::uintptr_t first =
            reinterpret_cast<std::uintptr_t>(base) + header_size;
        const std::uintptr_t aligned =
            (first + effective_alignment - 1) &
            ~(static_cast<std::uintptr_t>(effective_alignment) - 1);
        auto* header = reinterpret_cast<AllocationHeader*>(
            aligned - header_size);
        *header = {
            .base = base,
            .raw_size = static_cast<u64>(raw_size),
            .requested_size = static_cast<u64>(size),
            .magic = allocation_magic,
        };

        ++m_statistics.active_allocations;
        m_statistics.allocated_bytes += size;
        if (m_statistics.allocated_bytes >
            m_statistics.peak_allocated_bytes) {
            m_statistics.peak_allocated_bytes =
                m_statistics.allocated_bytes;
        }
        return reinterpret_cast<void*>(aligned);
    }

    void* VulkanHostAllocator::reallocate_impl(
        void* original,
        const std::size_t size,
        const std::size_t alignment) noexcept {
        if (original == nullptr)
            return allocate_impl(size, alignment);
        if (size == 0) {
            release_impl(original);
            return nullptr;
        }
        AllocationHeader* original_header = header_for(original);
        if (original_header->magic != allocation_magic)
            return nullptr;
        const u64 original_size = original_header->requested_size;
        void* replacement = allocate_impl(size, alignment);
        if (replacement == nullptr)
            return nullptr;
        std::memcpy(
            replacement,
            original,
            static_cast<std::size_t>(
                original_size < size ? original_size : size));
        release_impl(original);
        return replacement;
    }

    void VulkanHostAllocator::release_impl(void* allocation) noexcept {
        if (m_allocator == nullptr || allocation == nullptr)
            return;
        AllocationHeader* header = header_for(allocation);
        if (header->magic != allocation_magic || header->base == nullptr ||
            header->raw_size == 0 || header->requested_size == 0) {
            return;
        }

        auto lock = LockGuard::acquire(m_mutex);
        if (!lock)
            return;
        const AllocationHeader saved = *header;
        header->magic = 0;
        if (!m_allocator->_free_raw(saved.base, saved.raw_size)) {
            header->magic = allocation_magic;
            return;
        }
        if (m_statistics.active_allocations != 0)
            --m_statistics.active_allocations;
        m_statistics.allocated_bytes =
            saved.requested_size > m_statistics.allocated_bytes
                ? 0
                : m_statistics.allocated_bytes - saved.requested_size;
    }
}
