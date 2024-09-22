#include "nkpch.h"

#include "vulkan/image.h"

#include "vulkan/device.h"

namespace nk {
    void Image::init(const VulkanImageCreateInfo& create_info, Device* device, VkAllocationCallbacks* vulkan_allocator) {
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;
        m_extent = create_info.extent;
        m_format = create_info.format;

        // Creation info.
        VkImageCreateInfo image_create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.extent.width = m_extent.width;
        image_create_info.extent.height = m_extent.height;
        image_create_info.extent.depth = 1; // TODO: Support configurable depth.
        image_create_info.mipLevels = 4;    // TODO: Support mip mapping
        image_create_info.arrayLayers = 1;  // TODO: Support number of layers in the image.
        image_create_info.format = m_format;
        image_create_info.tiling = create_info.tiling;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_create_info.usage = create_info.usage;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;         // TODO: Configurable sample count.
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: Configurable sharing mode.

        VulkanCheck(vkCreateImage(m_device->get(), &image_create_info, m_vulkan_allocator, &m_image));

        // Query memory requirements.
        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(m_device->get(), m_image, &memory_requirements);

        u32 memory_type;
        if (!m_device->find_memory_index(memory_requirements.memoryTypeBits, create_info.memory_flags, &memory_type)) {
            ErrorLog("Required memory type not found. Image not valid.");
        }

        // Allocate memory
        VkMemoryAllocateInfo memory_allocate_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_type;
        VulkanCheck(vkAllocateMemory(m_device->get(), &memory_allocate_info, m_vulkan_allocator, &m_memory));

        // Bind the memory
        VulkanCheck(vkBindImageMemory(m_device->get(), m_image, m_memory, 0)); // TODO: configurable memory offset.

        // Create view
        if (create_info.create_view) {
            m_view = nullptr;
            create_view(create_info.view_aspect_flags);
        }
    }

    void Image::shutdown() {
        if (m_view != nullptr) {
            vkDestroyImageView(m_device->get(), m_view, m_vulkan_allocator);
            m_view = nullptr;
        }
        if (m_memory != nullptr) {
            vkFreeMemory(m_device->get(), m_memory, m_vulkan_allocator);
            m_memory = nullptr;
        }
        if (m_image != nullptr) {
            vkDestroyImage(m_device->get(), m_image, m_vulkan_allocator);
            m_image = nullptr;
        }
    }

    void Image::create_view(VkImageAspectFlags aspect_flags) {
        VkImageViewCreateInfo view_create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_create_info.image = m_image;
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // TODO: Make configurable.
        view_create_info.format = m_format;
        view_create_info.subresourceRange.aspectMask = aspect_flags;

        // TODO: Make configurable
        view_create_info.subresourceRange.baseMipLevel = 0;
        view_create_info.subresourceRange.levelCount = 1;
        view_create_info.subresourceRange.baseArrayLayer = 0;
        view_create_info.subresourceRange.layerCount = 1;

        VulkanCheck(vkCreateImageView(m_device->get(), &view_create_info, m_vulkan_allocator, &m_view));
    }
}
