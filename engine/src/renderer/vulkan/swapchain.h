#pragma once

#include "vulkan/vk.h"
#include "vulkan/image.h"
#include "vulkan/fence.h"
#include "core/arr.h"

namespace nk {
    class Device;
    class RenderPass;

    class Swapchain {
    public:
        Swapchain() = default;

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;

        Swapchain(Swapchain&& other);
        Swapchain& operator=(Swapchain&& other);

        void init(const u16 width, const u16 height, Device* device, VkAllocationCallbacks* allocator);
        void shutdown();

        void recreate(const u16 width, const u16 height);

        bool acquire_next_image_index(u64 timeout_ns, VkSemaphore image_available_semaphore, VkFence fence, const u16 width, const u16 height, u32* out_image_index);
        void present(VkQueue graphics_queue, VkQueue present_queue, VkSemaphore render_complete_semaphore, u32 present_image_index, const u16 width, const u16 height, bool* window_was_resized);

        const VkSurfaceFormatKHR& get_image_format() const { return m_image_format; }
        const u32 get_image_count() const { return m_images.length(); }
        Image& get_depth_attachment() { return m_depth_attachment; }
        VkImageView get_image_view_at(const u32 index) { return m_views[index]; }
        u32 get_max_frames_in_flight() const { return m_max_frames_in_flight; }
        u32 get_current_frame() const { return m_current_frame; }

    private:
        void create_swapchain(const u16 width, const u16 height);
        void destroy_swapchain();

        VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, const u16 width, const u16 height) const;
        VkSurfaceFormatKHR choose_swap_surface_format(const Arr<VkSurfaceFormatKHR>& available_formats) const;
        VkPresentModeKHR choose_swap_present_mode(const Arr<VkPresentModeKHR>& available_present_modes) const;

        VkAllocationCallbacks* m_allocator;
        Device* m_device;
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
