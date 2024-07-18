#pragma once

#include "vulkan/vk.h"
#include "vulkan/device.h"

namespace nk {
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

        Image(Image&& other);
        Image& operator=(Image&& other);

        void init(const VulkanImageCreateInfo& create_info, Device* device, VkAllocationCallbacks* allocator);
        void shutdown();

        void create_view(VkFormat format, VkImageAspectFlags aspect_flags);
        // void transition_layout(VkImageLayout old_layout, VkImageLayout new_layout, VkCommandBuffer command_buffer);
        // void copy_from_buffer(VkBuffer buffer, VkCommandBuffer command_buffer);

        VkImageView get_view() const { return m_view; }

    private:
        VkAllocationCallbacks* m_allocator;
        Device* m_device;

        VkImage m_image;
        VkDeviceMemory m_memory;
        VkImageView m_view;
        VkExtent2D m_extent;
        VkFormat m_format;
    };
}
