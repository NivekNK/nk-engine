#pragma once

#include "vulkan/vk.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Device;
    class CommandBuffer;

    struct VulkanImageCreateInfo {
        VkImageType image_type;
        VkExtent2D extent;
        VkFormat format;
        VkImageTiling tiling;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags memory_flags;
        bool create_view = false;
        VkImageAspectFlags view_aspect_flags = 0;
        u32 mip_levels = 1;
        u32 layer_count = 1;
        VkImageCreateFlags flags = 0;
        VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    };

    class Image {
    public:
        Image() = default;
        ~Image() { shutdown(); }

        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;
        Image(Image&& other) = delete;
        Image& operator=(Image&& other) = delete;

        [[nodiscard]] result<void, renderer_error> init(
            const VulkanImageCreateInfo& create_info,
            Device* device,
            VkAllocationCallbacks* allocator);
        [[nodiscard]] result<void, renderer_error> init_external(
            VkImage image,
            VkExtent2D extent,
            VkFormat format,
            VkImageAspectFlags view_aspect_flags,
            Device* device,
            VkAllocationCallbacks* allocator);
        void shutdown();

        [[nodiscard]] result<void, renderer_error> renew(
            const VulkanImageCreateInfo& create_info,
            Device* device,
            VkAllocationCallbacks* allocator) {
            shutdown();
            return init(create_info, device, allocator);
        }

        [[nodiscard]] result<void, renderer_error> create_view(
            VkImageAspectFlags aspect_flags);

        void copy_from_buffer(CommandBuffer* command_buffer, VkBuffer buffer);
        void copy_layers_from_buffer(
            CommandBuffer* command_buffer,
            VkBuffer buffer,
            u32 layer_count,
            u64 layer_size);
        void copy_from_buffer(
            CommandBuffer* command_buffer,
            VkBuffer buffer,
            u32 x,
            u32 y,
            u32 width,
            u32 height);
        // All levels must be transfer destinations, with level zero uploaded.
        // Returns every level in sampled use; even the single-level fallback.
        void generate_mipmaps(CommandBuffer& command_buffer);

        VkImageView get_view() const { return m_view; }
        VkImage get() const noexcept { return m_image; }

    private:
        Device* m_device = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;

        VkImage m_image = nullptr;
        VkDeviceMemory m_memory = nullptr;
        u64 m_memory_size = 0;
        u32 m_memory_index = 0;
        VkImageView m_view = nullptr;
        VkExtent2D m_extent{};
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        u32 m_mip_levels = 1;
        u32 m_layer_count = 1;
        VkImageViewType m_view_type = VK_IMAGE_VIEW_TYPE_2D;
        bool m_owns_image = false;
    };
}
