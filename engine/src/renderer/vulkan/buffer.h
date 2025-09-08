#pragma once

#include "vulkan/vk.h"

namespace nk {
    class Device;

    struct BufferCopyInfo {
        VkCommandPool pool;
        VkFence fence;
        VkQueue queue;
        VkBuffer source;
        u64 source_offset;
        VkBuffer destination;
        u64 destination_offset;
        u64 size;
    };

    class Buffer {
    public:
        Buffer() = default;
        ~Buffer() { shutdown(); }

        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&&) = delete;
        Buffer& operator=(Buffer&&) = delete;

        void init(
            Device* device,
            VkAllocationCallbacks* vulkan_allocator,
            u64 size,
            VkBufferUsageFlags usage,
            u32 memory_property_flags,
            bool bind_on_create
        );
        void shutdown();

        void resize(u64 size, VkQueue queue, VkCommandPool pool);

        void bind(u64 offset);

        void* lock_memory(u64 offset, u64 size, u32 flags);
        void unlock_memory();

        void load_data(u64 offset, u64 size, u32 flags, const void* data);

        void copy_to(const BufferCopyInfo& copy_info);

    private:
        Device* m_device;
        VkAllocationCallbacks* m_vulkan_allocator;

        u64 m_total_size;
        VkBuffer m_buffer;
        VkBufferUsageFlags m_usage;
        bool m_is_locked;
        VkDeviceMemory m_memory;
        u32 m_memory_index;
        u32 m_memory_property_flags;
    };
}
