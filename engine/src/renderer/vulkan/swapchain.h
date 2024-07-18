#pragma once

#include "vulkan/vk.h"
#include "vulkan/device.h"
#include "vulkan/image.h"
#include "core/arr.h"

namespace nk {
    class Swapchain {
    public:
        Swapchain() = default;

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;

        Swapchain(Swapchain&& other);
        Swapchain& operator=(Swapchain&& other);

        void init(const u16 width, const u16 height, Device& device, VkAllocationCallbacks* allocator);
        void shutdown(Device& device);

        const VkSurfaceFormatKHR& get_image_format() const { return m_image_format; }
        const u32 get_image_count() const { return m_images.length(); }

    private:
        void create_swapchain(const u16 width, const u16 height, Device& device);
        void destroy_swapchain(Device& device);

        VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, const u16 width, const u16 height) const;
        VkSurfaceFormatKHR choose_swap_surface_format(const Arr<VkSurfaceFormatKHR>& available_formats) const;
        VkPresentModeKHR choose_swap_present_mode(const Arr<VkPresentModeKHR>& available_present_modes) const;

        VkAllocationCallbacks* m_allocator;
        Allocator* m_swapchain_allocator;

        u32 m_max_frames_in_flight;
        VkSwapchainKHR m_swapchain;
        u32 m_current_frame;

        Arr<VkImage> m_images;
        Arr<VkImageView> m_views;

        Image m_depth_attachment;
        VkSurfaceFormatKHR m_image_format;
        VkExtent2D m_swapchain_extent;
    };
}
