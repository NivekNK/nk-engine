#pragma once

#include "vulkan/vk.h"
#include "core/result.h"
#include "renderer/render_buffer.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Device;

    struct BufferCopyInfo {
        VkCommandPool pool;
        VkQueue queue;
        VkBuffer source;
        u64 source_offset;
        VkBuffer destination;
        u64 destination_offset;
        u64 size;
        VkPipelineStageFlags destination_stages;
        VkAccessFlags destination_access;
    };

    class Buffer final : public RenderBuffer {
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
            const RenderBufferConfig& config,
            mem::Allocator* suballocation_metadata = nullptr) noexcept;
        void shutdown() noexcept;

        VkBuffer get() const { return m_buffer; }
        VkBuffer operator()() { return m_buffer; }
        operator VkBuffer() { return m_buffer; }

    private:
        [[nodiscard]] result<void, renderer_error> create_backend(
            const RenderBufferConfig& config) noexcept override;
        void destroy_backend() noexcept override;
        [[nodiscard]] result<void*, renderer_error> map_backend(
            u64 offset,
            u64 size) noexcept override;
        void unmap_backend() noexcept override;
        [[nodiscard]] result<void, renderer_error> flush_backend(
            u64 offset,
            u64 size) noexcept override;
        [[nodiscard]] result<void, renderer_error> upload_backend(
            u64 offset,
            u64 size,
            const void* data) noexcept override;
        [[nodiscard]] result<void, renderer_error> copy_backend(
            u64 source_offset,
            RenderBuffer& destination,
            u64 destination_offset,
            u64 size) noexcept override;
        [[nodiscard]] result<void, renderer_error> resize_backend(
            u64 size) noexcept override;
        [[nodiscard]] result<void, renderer_error> copy_native(
            const BufferCopyInfo& copy_info) noexcept;

        Device* m_device = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;

        u64 m_total_size = 0;
        VkBuffer m_buffer = nullptr;
        VkBufferUsageFlags m_usage = 0;
        bool m_is_locked = false;
        bool m_is_bound = false;
        bool m_persistently_mapped = false;
        void* m_mapped_data = nullptr;
        VkDeviceMemory m_memory = nullptr;
        u64 m_memory_size = 0;
        u32 m_memory_index = 0;
        u32 m_memory_property_flags = 0;
    };
}
