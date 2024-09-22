#pragma once

#include "vulkan/vk.h"
#include "core/dyarr.h"
#include "core/arr.h"
#include "vulkan/image.h"

namespace nk {
    class Device;

    class Swapchain {
    public:
        Swapchain() = default;

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        void init(u16& width, u16& height, Device* device, Allocator* allocator, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        void recreate(u16& width, u16& height);

        bool acquire_next_image_index(
            u16& framebuffer_width,
            u16& framebuffer_height,
            u64 timeout_ns,
            VkSemaphore image_available_semaphore,
            VkFence fence,
            u32* out_image_index);

        void present(
            u16& framebuffer_width,
            u16& framebuffer_height,
            VkQueue graphics_queue,
            VkQueue present_queue,
            VkSemaphore render_complete_semaphore,
            u32 present_image_index);

        VkSurfaceFormatKHR get_image_format() const { return m_image_format; }
        u32 get_image_count() const { return m_images.length(); }
        VkImageView get_image_view_at(u32 index) { return m_views[index]; }
        Image& get_depth_attachment() { return m_depth_attachment; }
        u8 get_max_frames_in_flight() const { return m_max_frames_in_flight; }
        const u32& get_current_frame() const { return m_current_frame; }

    private:
        VkSurfaceFormatKHR choose_swap_surface_format(const Dyarr<VkSurfaceFormatKHR>& available_formats) const;
        VkPresentModeKHR choose_swap_present_mode(const Dyarr<VkPresentModeKHR>& available_present_modes) const;

        void create_swapchain(u16& width, u16& height);
        void destroy_swapchain();

        Device* m_device;
        Allocator* m_allocator;
        VkAllocationCallbacks* m_vulkan_allocator;

        VkSurfaceFormatKHR m_image_format;
        u8 m_max_frames_in_flight;
        VkSwapchainKHR m_swapchain;
        Arr<VkImage> m_images;
        Arr<VkImageView> m_views;

        u32 m_current_frame;
        Image m_depth_attachment;
    };
}
