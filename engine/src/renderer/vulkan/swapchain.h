#pragma once

#include "vulkan/vk.h"
#include "vulkan/image.h"

namespace nk {
    class Device;
    namespace mem { class Allocator; }

    class Swapchain {
    public:
        Swapchain() = default;
        ~Swapchain() = default;

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        void init(u32& width, u32& height, u32* current_frame, Device* device, mem::Allocator* allocator, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        VkSurfaceFormatKHR get_image_format() const { return m_image_format; }
        u32 get_image_count() const { return m_image_count; }
        VkImageView get_image_view_at(u32 index) {
            if (index >= m_image_count) {
                ErrorLog("nk::Swapchain::get_image_view_at Index '{}' out of bounds!", index);
                return nullptr;
            }
            return m_views[index];
        }
        Image* get_depth_attachment() { return &m_depth_attachment; }
        u8 get_max_frames_in_flight() const { return m_max_frames_in_flight; }

    private:
        void create_swapchain(u32& width, u32& height);
        void destroy_swapchain();

        // TODO: Change it for a safe way of storing a pointer like a ref counter
        u32* m_current_frame;
        Device* m_device;
        mem::Allocator* m_allocator;
        VkAllocationCallbacks* m_vulkan_allocator;

        VkSurfaceFormatKHR m_image_format;
        u8 m_max_frames_in_flight;
        VkSwapchainKHR m_swapchain;
        u32 m_image_count;
        VkImage* m_images;
        VkImageView* m_views;
        Image m_depth_attachment;
    };
}
