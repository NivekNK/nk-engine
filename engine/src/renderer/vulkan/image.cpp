#include "nkpch.h"

#include "vulkan/image.h"

#include "vulkan/device.h"
#include "vulkan/command_buffer.h"
#include "vulkan/graphics_commands.h"
#include "vulkan/image_utils.h"

namespace nk {
    result<void, renderer_error> Image::init(
        const VulkanImageCreateInfo& create_info,
        Device* device,
        VkAllocationCallbacks* vulkan_allocator) {
        if (create_info.mip_levels == 0 ||
            create_info.mip_levels > vk::mip_level_count(create_info.extent))
            return err(renderer_error{renderer_error_code::image_creation_failed, 0});
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;
        m_extent = create_info.extent;
        m_format = create_info.format;
        m_mip_levels = create_info.mip_levels;

        // Creation info.
        VkImageCreateInfo image_create_info = {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.extent.width = m_extent.width;
        image_create_info.extent.height = m_extent.height;
        image_create_info.extent.depth = 1; // TODO: Support configurable depth.
        image_create_info.mipLevels = m_mip_levels;
        image_create_info.arrayLayers = 1;  // TODO: Support number of layers in the image.
        image_create_info.format = m_format;
        image_create_info.tiling = create_info.tiling;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_create_info.usage = create_info.usage;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;         // TODO: Configurable sample count.
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: Configurable sharing mode.

        VkResult result = vkCreateImage(
            m_device->get(), &image_create_info, m_vulkan_allocator, &m_image);
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::image_creation_failed,
                .native_code = static_cast<i32>(result),
            });
        m_owns_image = true;

        // Query memory requirements.
        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(m_device->get(), m_image, &memory_requirements);

        u32 memory_type;
        if (!m_device->find_memory_index(memory_requirements.memoryTypeBits, create_info.memory_flags, &memory_type)) {
            shutdown();
            return err(renderer_error{
                .code = renderer_error_code::image_memory_failed,
                .native_code = 0,
            });
        }

        // Allocate memory
        VkMemoryAllocateInfo memory_allocate_info = {};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_type;
        result = vkAllocateMemory(
            m_device->get(), &memory_allocate_info, m_vulkan_allocator, &m_memory);
        if (result != VK_SUCCESS) {
            shutdown();
            return err(renderer_error{
                .code = renderer_error_code::image_memory_failed,
                .native_code = static_cast<i32>(result),
            });
        }

        // Bind the memory
        result = vkBindImageMemory(m_device->get(), m_image, m_memory, 0);
        if (result != VK_SUCCESS) {
            shutdown();
            return err(renderer_error{
                .code = renderer_error_code::image_memory_failed,
                .native_code = static_cast<i32>(result),
            });
        }

        // Create view
        if (create_info.create_view) {
            m_view = nullptr;
            auto view_created = create_view(create_info.view_aspect_flags);
            if (!view_created) {
                shutdown();
                return err(view_created.error());
            }
        }
        return ok();
    }

    result<void, renderer_error> Image::init_external(
        const VkImage image,
        const VkExtent2D extent,
        const VkFormat format,
        const VkImageAspectFlags view_aspect_flags,
        Device* device,
        VkAllocationCallbacks* allocator) {
        if (image == VK_NULL_HANDLE || extent.width == 0 ||
            extent.height == 0 || format == VK_FORMAT_UNDEFINED ||
            view_aspect_flags == 0 || device == nullptr ||
            device->get() == VK_NULL_HANDLE || m_image != VK_NULL_HANDLE ||
            m_view != VK_NULL_HANDLE || m_memory != VK_NULL_HANDLE) {
            return err(renderer_error{
                renderer_error_code::image_creation_failed,
                0,
            });
        }

        m_device = device;
        m_vulkan_allocator = allocator;
        m_image = image;
        m_extent = extent;
        m_format = format;
        m_mip_levels = 1;
        m_owns_image = false;
        auto view_created = create_view(view_aspect_flags);
        if (!view_created) {
            const renderer_error error = view_created.error();
            shutdown();
            return err(error);
        }
        return ok();
    }

    void Image::shutdown() {
        if (m_view != nullptr) {
            vkDestroyImageView(m_device->get(), m_view, m_vulkan_allocator);
            m_view = nullptr;
        }
        if (m_image != nullptr && m_owns_image) {
            vkDestroyImage(m_device->get(), m_image, m_vulkan_allocator);
        }
        m_image = nullptr;
        if (m_memory != nullptr) {
            vkFreeMemory(m_device->get(), m_memory, m_vulkan_allocator);
            m_memory = nullptr;
        }
        m_device = nullptr;
        m_vulkan_allocator = nullptr;
        m_extent = {};
        m_format = VK_FORMAT_UNDEFINED;
        m_mip_levels = 1;
        m_owns_image = false;
    }

    result<void, renderer_error> Image::create_view(
        VkImageAspectFlags aspect_flags) {
        VkImageViewCreateInfo view_create_info = {};
        view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.image = m_image;
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // TODO: Make configurable.
        view_create_info.format = m_format;
        view_create_info.subresourceRange.aspectMask = aspect_flags;

        // TODO: Make configurable
        view_create_info.subresourceRange.baseMipLevel = 0;
        view_create_info.subresourceRange.levelCount = m_mip_levels;
        view_create_info.subresourceRange.baseArrayLayer = 0;
        view_create_info.subresourceRange.layerCount = 1;

        const VkResult result = vkCreateImageView(
            m_device->get(), &view_create_info, m_vulkan_allocator, &m_view);
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::image_view_creation_failed,
                .native_code = static_cast<i32>(result),
            });
        return ok();
    }

    void Image::generate_mipmaps(CommandBuffer& command_buffer) {
        const vk::GraphicsCommands commands{*m_device, command_buffer};
        for (u32 level = 1; level < m_mip_levels; ++level) {
            const VkImageSubresourceRange previous{VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1};
            commands.transition(m_image, previous,
                vk::ImageUse::transfer_destination, vk::ImageUse::transfer_source);
            const VkExtent2D source = vk::mip_extent(m_extent, level - 1);
            const VkExtent2D destination = vk::mip_extent(m_extent, level);
            VkImageBlit region{};
            region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1};
            region.srcOffsets[1] = {static_cast<i32>(source.width), static_cast<i32>(source.height), 1};
            region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
            region.dstOffsets[1] = {static_cast<i32>(destination.width), static_cast<i32>(destination.height), 1};
            vkCmdBlitImage(command_buffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);
            commands.transition(m_image, previous,
                vk::ImageUse::transfer_source, vk::ImageUse::sampled);
        }
        commands.transition(m_image, {VK_IMAGE_ASPECT_COLOR_BIT, m_mip_levels - 1, 1, 0, 1},
            vk::ImageUse::transfer_destination, vk::ImageUse::sampled);
    }

    void Image::copy_from_buffer(CommandBuffer* command_buffer, VkBuffer buffer) {
        copy_from_buffer(
            command_buffer,
            buffer,
            0,
            0,
            m_extent.width,
            m_extent.height);
    }

    void Image::copy_from_buffer(
        CommandBuffer* command_buffer,
        const VkBuffer buffer,
        const u32 x,
        const u32 y,
        const u32 width,
        const u32 height) {
        // Region to copy
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {
            static_cast<i32>(x),
            static_cast<i32>(y),
            0,
        };
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(
            command_buffer->get(),
            buffer,
            m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);
    }
}
