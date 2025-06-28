#include "nkpch.h"

#include "vulkan/swapchain.h"

#include "collections/dyarr.h"
#include "vulkan/device.h"
#include "vulkan/utils.h"

namespace nk {
    VkSurfaceFormatKHR choose_swap_surface_format(const cl::dyarr<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR choose_swap_present_mode(const cl::dyarr<VkPresentModeKHR>& available_present_modes);

    void Swapchain::init(u32& width, u32& height, u32* current_frame, Device* device, mem::Allocator* allocator, VkAllocationCallbacks* vulkan_allocator) {
        m_current_frame = current_frame;
        m_device = device;
        m_allocator = allocator;
        m_vulkan_allocator = vulkan_allocator;
        create_swapchain(width, height);
        TraceLog("nk::Swapchain initialized.");
    }

    void Swapchain::shutdown() {
        destroy_swapchain();
        if (m_images) {
            m_allocator->free_lot_t(VkImage, m_images, m_image_count);
        }
        if (m_views) {
            m_allocator->free_lot_t(VkImageView, m_views, m_image_count);
        }
        TraceLog("nk::Swapchain shutdown.");
    }

    void Swapchain::recreate(u32& width, u32& height) {
        destroy_swapchain();
        create_swapchain(width, height);
    }

    bool Swapchain::acquire_next_image_index(
        u32* out_image_index,
        u32& framebuffer_width,
        u32& framebuffer_height,
        u64 timeout_ns,
        VkSemaphore image_available_semaphore,
        VkFence fence) {
        VkResult result = vkAcquireNextImageKHR(
            m_device->get(),
            m_swapchain,
            timeout_ns,
            image_available_semaphore,
            fence,
            out_image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate(framebuffer_width, framebuffer_height);
            return false;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            FatalLog("Failed to acquire Swapchain image! Result: {}", vk::result_to_cstr(result, true));
            return false;
        }

        return true;
    }

    void Swapchain::present(
        u32& framebuffer_width,
        u32& framebuffer_height,
        VkQueue graphics_queue,
        VkQueue present_queue,
        VkSemaphore render_complete_semaphore,
        u32 present_image_index) {
        // Return the image to the swapchain for presentation.
        VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_complete_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &m_swapchain;
        present_info.pImageIndices = &present_image_index;
        present_info.pResults = nullptr;

        VkResult result = vkQueuePresentKHR(present_queue, &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            // Swapchain is out of date, suboptimal or a framebuffer resize has occurred.
            // Trigger swapchain recreation.
            recreate(framebuffer_width, framebuffer_height);
        } else if (result != VK_SUCCESS) {
            FatalLog("Failed to Present Swap Chain image! {}", vk::result_to_cstr(result, true));
        }

        // Increment (and loop) the index.
        *m_current_frame = (*m_current_frame + 1) % m_max_frames_in_flight;
    }

    void Swapchain::create_swapchain(u32& width, u32& height) {
        VkExtent2D swapchain_extent = {width, height};
        const SwapchainSupportInfo& swapchain_support_info = m_device->query_swapchain_support_info();
        m_image_format = choose_swap_surface_format(swapchain_support_info.formats);
        VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support_info.present_modes);

        if (swapchain_support_info.capabilities.currentExtent.width != numeric::u32_max) {
            swapchain_extent = swapchain_support_info.capabilities.currentExtent;
        }

        VkExtent2D min = swapchain_support_info.capabilities.minImageExtent;
        VkExtent2D max = swapchain_support_info.capabilities.maxImageExtent;
        swapchain_extent.width = Clamp(swapchain_extent.width, min.width, max.width);
        swapchain_extent.height = Clamp(swapchain_extent.height, min.height, max.height);

        u32 image_count = swapchain_support_info.capabilities.minImageCount + 1;
        if (swapchain_support_info.capabilities.maxImageCount > 0 &&
            image_count > swapchain_support_info.capabilities.maxImageCount) {
            image_count = swapchain_support_info.capabilities.maxImageCount;
        }

        m_max_frames_in_flight = image_count - 1;

        // Swapchain create info
        VkSwapchainCreateInfoKHR swapchain_create_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        swapchain_create_info.surface = m_device->get_surface();
        swapchain_create_info.minImageCount = image_count;
        swapchain_create_info.imageFormat = m_image_format.format;
        swapchain_create_info.imageColorSpace = m_image_format.colorSpace;
        swapchain_create_info.imageExtent = swapchain_extent;
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const PhysicalDeviceQueueFamilyInfo& queue_family = m_device->get_queue_family_info();
        if (queue_family.graphics_family_index != queue_family.present_family_index) {
            u32 queue_family_indices[2] = {
                queue_family.graphics_family_index,
                queue_family.present_family_index};
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_create_info.queueFamilyIndexCount = 2;
            swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
        } else {
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchain_create_info.queueFamilyIndexCount = 0;
            swapchain_create_info.pQueueFamilyIndices = nullptr;
        }

        swapchain_create_info.preTransform = swapchain_support_info.capabilities.currentTransform;
        swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_create_info.presentMode = present_mode;
        swapchain_create_info.clipped = VK_TRUE;
        swapchain_create_info.oldSwapchain = nullptr;

        VulkanCheck(vkCreateSwapchainKHR(m_device->get(), &swapchain_create_info, m_vulkan_allocator, &m_swapchain));

        *m_current_frame = 0;

        m_image_count = 0;
        VulkanCheck(vkGetSwapchainImagesKHR(m_device->get(), m_swapchain, &m_image_count, nullptr));
        if (!m_images) {
            m_images = m_allocator->allocate_lot_t(VkImage, m_image_count);
        }
        if (!m_views) {
            m_views = m_allocator->allocate_lot_t(VkImageView, m_image_count);
        }
        VulkanCheck(vkGetSwapchainImagesKHR(m_device->get(), m_swapchain, &m_image_count, m_images));

        for (u32 i = 0; i < m_image_count; i++) {
            VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view_info.image = m_images[i];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = m_image_format.format;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;
            VulkanCheck(vkCreateImageView(m_device->get(), &view_info, m_vulkan_allocator, &m_views[i]));
        }

        // Create depth image and its view.
        auto depth_create_info = VulkanImageCreateInfo{
            .image_type = VK_IMAGE_TYPE_2D,
            .extent = {swapchain_extent.width, swapchain_extent.height},
            .format = m_device->get_depth_format(),
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .create_view = true,
            .view_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT,
        };
        m_depth_attachment.init(depth_create_info, m_device, m_vulkan_allocator);

        width = swapchain_extent.width;
        height = swapchain_extent.height;
    }

    void Swapchain::destroy_swapchain() {
        vkDeviceWaitIdle(m_device->get());
        m_depth_attachment.shutdown();

        // Only destroy the views, not the images, since those are owned by the swapchain and are thus
        // destroyed when it is.
        for (u32 i = 0; i < m_image_count; i++) {
            vkDestroyImageView(m_device->get(), m_views[i], m_vulkan_allocator);
        }

        vkDestroySwapchainKHR(m_device->get(), m_swapchain, m_vulkan_allocator);
    }

    VkSurfaceFormatKHR choose_swap_surface_format(const cl::dyarr<VkSurfaceFormatKHR>& available_formats) {
        for (u64 i = 0; i < available_formats.length(); i++) {
            if (available_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
                available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return available_formats[i];
            }
        }
        return available_formats[0];
    }

    VkPresentModeKHR choose_swap_present_mode(const cl::dyarr<VkPresentModeKHR>& available_present_modes) {
        for (u64 i = 0; i < available_present_modes.length(); i++) {
            if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                return available_present_modes[i];
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
}
