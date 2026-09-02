#pragma once

#include "vulkan/vk.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

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

        [[nodiscard]] result<void, renderer_error> init(
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

        [[nodiscard]] result<void, renderer_error> copy_to(
            const BufferCopyInfo& copy_info);

        VkBuffer get() const { return m_buffer; }
        u64 size() const noexcept { return m_total_size; }
        VkBuffer operator()() { return m_buffer; }
        operator VkBuffer() { return m_buffer; }

    private:
        Device* m_device = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;

        u64 m_total_size = 0;
        VkBuffer m_buffer = nullptr;
        VkBufferUsageFlags m_usage = 0;
        bool m_is_locked = false;
        VkDeviceMemory m_memory = nullptr;
        u32 m_memory_index = 0;
        u32 m_memory_property_flags = 0;
    };
}
