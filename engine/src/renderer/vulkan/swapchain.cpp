#include "nkpch.h"

#include "vulkan/swapchain.h"

#include "vulkan/device.h"
#include "vulkan/render_pass.h"
#include "memory/malloc_allocator.h"

namespace nk {
    Swapchain::Swapchain(Swapchain&& other) {
        m_allocator = other.m_allocator;
        m_swapchain_allocator = other.m_swapchain_allocator;
        m_max_frames_in_flight = other.m_max_frames_in_flight;
        m_swapchain = other.m_swapchain;
        m_current_frame = other.m_current_frame;
        m_image_format = other.m_image_format;

        other.m_allocator = nullptr;
        other.m_swapchain_allocator = nullptr;
        other.m_max_frames_in_flight = 0;
        other.m_swapchain = nullptr;
        other.m_current_frame = 0;
        other.m_image_format = {};
        other.m_swapchain_extent = {};

        m_images = std::move(other.m_images);
        m_views = std::move(other.m_views);

        m_depth_attachment = std::move(other.m_depth_attachment);
    }

    Swapchain& Swapchain::operator=(Swapchain&& other) {
        m_allocator = other.m_allocator;
        m_swapchain_allocator = other.m_swapchain_allocator;
        m_max_frames_in_flight = other.m_max_frames_in_flight;
        m_swapchain = other.m_swapchain;
        m_current_frame = other.m_current_frame;

        other.m_allocator = nullptr;
        other.m_swapchain_allocator = nullptr;
        other.m_max_frames_in_flight = 0;
        other.m_swapchain = nullptr;
        other.m_current_frame = 0;

        m_images = std::move(other.m_images);
        m_views = std::move(other.m_views);

        return *this;
    }

    void Swapchain::init(const u16 width, const u16 height, Device* device, VkAllocationCallbacks* allocator) {
        m_allocator = allocator;
        m_device = device;

        auto swapchain_allocator = new MallocAllocator();
        swapchain_allocator->allocator_init("SwapchainAllocator", MemoryType::Renderer);
        m_swapchain_allocator = swapchain_allocator;

        create_swapchain(width, height);
        TraceLog("nk::Swapchain initialized.");
    }

    void Swapchain::shutdown() {
        destroy_swapchain();
        delete m_swapchain_allocator;

        TraceLog("nk::Swapchain shutdown.");
    }

    void Swapchain::recreate(const u16 width, const u16 height) {
        vkDeviceWaitIdle(m_device->get());
        destroy_swapchain();
        create_swapchain(width, height);
    }

    bool Swapchain::acquire_next_image_index(u64 timeout_ns, VkSemaphore image_available_semaphore, VkFence fence, const u16 width, const u16 height, u32* out_image_index) {
        VkResult result = vkAcquireNextImageKHR(
            m_device->get(),
            m_swapchain,
            timeout_ns,
            image_available_semaphore,
            fence,
            out_image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Trigger swapchain recreation, then boot out of the render loop.
            recreate(width, height);
            return false;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            FatalLog("Failed to acquire Swap Chain image!");
            return false;
        }

        return true;
    }

    void Swapchain::present(VkQueue graphics_queue, VkQueue present_queue, VkSemaphore render_complete_semaphore, u32 present_image_index, const u16 width, const u16 height, bool* window_was_resized) {
        // Return the image to the swapchain for presentation.
        VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_complete_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &m_swapchain;
        present_info.pImageIndices = &present_image_index;
        present_info.pResults = nullptr;

        VkResult result = vkQueuePresentKHR(present_queue, &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || *window_was_resized) {
            // Swapchain is out of date, suboptimal or a framebuffer resize has occurred. Trigger swapchain recreation.
            // NOTE: before used context->framebuffer_width, context->framebuffer_height as parameters
            *window_was_resized = false;
            recreate(width, height);
        } else if (result != VK_SUCCESS) {
            FatalLog("Failed to Present Swap Chain image!");
        }

        // Increment (and loop) the index.
        m_current_frame = (m_current_frame + 1) % m_max_frames_in_flight;
    }

    void Swapchain::create_swapchain(const u16 width, const u16 height) {
        const SwapchainSupportInfo& swapchain_support_info = m_device->get_swapchain_support_info();

        VkSurfaceFormatKHR image_format = choose_swap_surface_format(swapchain_support_info.formats);
        VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support_info.present_modes);
        VkExtent2D swapchain_extent = choose_swap_extent(swapchain_support_info.capabilities, width, height);

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
        swapchain_create_info.imageFormat = image_format.format;
        swapchain_create_info.imageColorSpace = image_format.colorSpace;
        swapchain_create_info.imageExtent = swapchain_extent;
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Setup the queue family indices
        const PhysicalDeviceQueueFamilyInfo& queue_family = m_device->get_queue_family_info();
        if (queue_family.graphics_family_index != queue_family.present_family_index) {
            u32 queue_family_indices[] = {
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
        swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

        VulkanCheck(vkCreateSwapchainKHR(m_device->get(), &swapchain_create_info, m_allocator, &m_swapchain));

        // Start with a zero frame index.
        m_current_frame = 0;

        // Images
        VulkanCheck(vkGetSwapchainImagesKHR(m_device->get(), m_swapchain, &image_count, nullptr));
        m_images.init(m_swapchain_allocator, image_count);
        VulkanCheck(vkGetSwapchainImagesKHR(m_device->get(), m_swapchain, &image_count, m_images.data()));

        // Views
        m_views.init(m_swapchain_allocator, image_count);
        for (u32 i = 0; i < image_count; i++) {
            VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view_info.image = m_images[i];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = image_format.format;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            VulkanCheck(vkCreateImageView(m_device->get(), &view_info, m_allocator, &m_views[i]));
        }

        // Depth resources
        const VkFormat depth_format = m_device->get_depth_format();
        if (depth_format == VK_FORMAT_UNDEFINED) {
            FatalLog("Failed to find a supported depth format!");
        }

        // Create depth image and its view.
        auto depth_create_info = VulkanImageCreateInfo{
            .image_type = VK_IMAGE_TYPE_2D,
            .extent = {width, height},
            .format = depth_format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .memory_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .create_view = true,
            .view_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT,
        };

        m_depth_attachment.init(depth_create_info, m_device, m_allocator);
        m_image_format = image_format;
        m_swapchain_extent = swapchain_extent;
    }

    void Swapchain::destroy_swapchain() {
        m_depth_attachment.shutdown();

        // Only destroy the views, not the images, since those are owned by the swapchain and are thus
        // destroyed when it is.
        for (auto& view : m_views) {
            vkDestroyImageView(m_device->get(), view, m_allocator);
        }
        m_views.clear();

        vkDestroySwapchainKHR(m_device->get(), m_swapchain, m_allocator);

        m_images.clear();
    }

    VkExtent2D Swapchain::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, const u16 width, const u16 height) const {
        if (capabilities.currentExtent.width != u32_max)
            return capabilities.currentExtent;

        VkExtent2D current_extent = {width, height};
        current_extent.width = MaxValue(
            capabilities.minImageExtent.width,
            MinValue(capabilities.maxImageExtent.width, current_extent.width));
        current_extent.height = MaxValue(
            capabilities.minImageExtent.height,
            MinValue(capabilities.maxImageExtent.height, current_extent.height));

        return current_extent;
    }

    VkSurfaceFormatKHR Swapchain::choose_swap_surface_format(const Arr<VkSurfaceFormatKHR>& available_formats) const {
        for (const auto& available_format : available_formats) {
            if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return available_format;
            }
        }
        return available_formats[0];
    }

    VkPresentModeKHR Swapchain::choose_swap_present_mode(const Arr<VkPresentModeKHR>& available_present_modes) const {
        for (const auto& available_present_mode : available_present_modes) {
            if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return available_present_mode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
}
