#pragma once

#include "vulkan/vk.h"

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
    };

    class Image {
    public:
        Image() = default;
        ~Image() { shutdown(); }

        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;
        Image(Image&& other) = delete;
        Image& operator=(Image&& other) = delete;

        void init(const VulkanImageCreateInfo& create_info, Device* device, VkAllocationCallbacks* allocator);
        void shutdown();

        void renew(const VulkanImageCreateInfo& create_info, Device* device, VkAllocationCallbacks* allocator) {
            shutdown();
            init(create_info, device, allocator);
        }

        void create_view(VkImageAspectFlags aspect_flags);

        void transition_layout(CommandBuffer* command_buffer, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);

        void copy_from_buffer(CommandBuffer* command_buffer,VkBuffer buffer);

        VkImageView get_view() const { return m_view; }

    private:
        Device* m_device;
        VkAllocationCallbacks* m_vulkan_allocator;

        VkImage m_image;
        VkDeviceMemory m_memory;
        VkImageView m_view;
        VkExtent2D m_extent;
        VkFormat m_format;
    };
}
