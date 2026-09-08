#pragma once

#include "core/thread.h"
#include "memory/allocator.h"
#include "vulkan/vk.h"

namespace nk::vk {
    struct VulkanHostAllocatorStatistics {
        u64 active_allocations = 0;
        u64 allocated_bytes = 0;
        u64 peak_allocated_bytes = 0;
        u64 internal_allocations = 0;
        u64 internal_bytes = 0;
    };

    // Optional VkAllocationCallbacks implementation for diagnostics. It tracks
    // driver-side host allocations, not VkDeviceMemory. Release builds keep
    // using nullptr callbacks unless explicitly enabled by the application.
    class VulkanHostAllocator final {
    public:
        VulkanHostAllocator() noexcept;
        ~VulkanHostAllocator();

        VulkanHostAllocator(const VulkanHostAllocator&) = delete;
        VulkanHostAllocator& operator=(const VulkanHostAllocator&) = delete;
        VulkanHostAllocator(VulkanHostAllocator&&) = delete;
        VulkanHostAllocator& operator=(VulkanHostAllocator&&) = delete;

        [[nodiscard]] bool init(mem::Allocator& allocator) noexcept;
        [[nodiscard]] bool shutdown() noexcept;

        [[nodiscard]] VkAllocationCallbacks* callbacks() noexcept {
            return m_allocator == nullptr ? nullptr : &m_callbacks;
        }
        [[nodiscard]] VulkanHostAllocatorStatistics statistics() const noexcept;

    private:
        static VKAPI_ATTR void* VKAPI_CALL allocate(
            void* user_data,
            std::size_t size,
            std::size_t alignment,
            VkSystemAllocationScope scope);
        static VKAPI_ATTR void* VKAPI_CALL reallocate(
            void* user_data,
            void* original,
            std::size_t size,
            std::size_t alignment,
            VkSystemAllocationScope scope);
        static VKAPI_ATTR void VKAPI_CALL release(
            void* user_data,
            void* allocation);
        static VKAPI_ATTR void VKAPI_CALL internal_allocate(
            void* user_data,
            std::size_t size,
            VkInternalAllocationType type,
            VkSystemAllocationScope scope);
        static VKAPI_ATTR void VKAPI_CALL internal_release(
            void* user_data,
            std::size_t size,
            VkInternalAllocationType type,
            VkSystemAllocationScope scope);

        [[nodiscard]] void* allocate_impl(
            std::size_t size,
            std::size_t alignment) noexcept;
        [[nodiscard]] void* reallocate_impl(
            void* original,
            std::size_t size,
            std::size_t alignment) noexcept;
        void release_impl(void* allocation) noexcept;

        mem::Allocator* m_allocator = nullptr;
        VkAllocationCallbacks m_callbacks{};
        mutable Mutex m_mutex;
        VulkanHostAllocatorStatistics m_statistics{};
    };
}
