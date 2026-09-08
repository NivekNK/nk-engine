#pragma once

#include "vulkan/vk.h"
#include "vulkan/image.h"
#include "collections/arr.h"
#include "core/result.h"
#include "renderer/renderer_result.h"
#include "resources/texture.h"
#include "vulkan/resources/texture_data.h"

namespace nk {
    class Device;
    namespace mem {
        class Allocator;
    }

    class Swapchain {
    public:
        Swapchain() = default;
        ~Swapchain() = default;

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        [[nodiscard]] result<void, renderer_error> init(
            u32& width,
            u32& height,
            u32* current_frame,
            Device* device,
            mem::Allocator* allocator,
            VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        [[nodiscard]] result<void, renderer_error> recreate(
            u32& width,
            u32& height);

        [[nodiscard]] result<swapchain_outcome, renderer_error>
        acquire_next_image_index(
            u32* out_image_index,
            u64 timeout_ns,
            VkSemaphore image_available_semaphore,
            VkFence fence);

        [[nodiscard]] result<swapchain_outcome, renderer_error> present(
            VkQueue present_queue,
            VkSemaphore render_complete_semaphore,
            u32 present_image_index);

        VkSurfaceFormatKHR get_image_format() const { return m_image_format; }
        VkImage get_image_at(u32 index) const {
            return m_texture_data[index].image.get();
        }
        Texture* get_render_texture_at(u32 index) {
            return index < m_image_count ? &m_render_textures[index] : nullptr;
        }
        Texture* get_depth_texture_at(u32 index) {
            return index < m_image_count ? &m_depth_textures[index] : nullptr;
        }
        const Texture* get_render_texture_at(u32 index) const {
            return index < m_image_count ? &m_render_textures[index] : nullptr;
        }
        bool is_initialized() const { return m_swapchain != nullptr; }
        u32 get_image_count() const { return m_image_count; }
        VkImageView get_image_view_at(u32 index) {
            if (index >= m_image_count) {
                ErrorLog("nk::Swapchain::get_image_view_at Index '{}' out of bounds!", index);
                return nullptr;
            }
            return m_texture_data[index].image.get_view();
        }
        Image* get_depth_attachment(u32 image_index) {
            return &m_depth_texture_data[image_index].image;
        }
        u8 get_max_frames_in_flight() const { return m_max_frames_in_flight; }

    private:
        [[nodiscard]] result<void, renderer_error> create_swapchain(
            u32& width,
            u32& height);
        void destroy_swapchain();

        // TODO: Change it for a safe way of storing a pointer like a ref counter
        u32* m_current_frame = nullptr;
        Device* m_device = nullptr;
        mem::Allocator* m_allocator = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;

        VkSurfaceFormatKHR m_image_format{};
        u8 m_max_frames_in_flight = 0;
        VkSwapchainKHR m_swapchain = nullptr;
        u32 m_image_count = 0;
        cl::arr<VkImage> m_images;
        cl::arr<Texture> m_render_textures;
        cl::arr<TextureData> m_texture_data;
        cl::arr<Texture> m_depth_textures;
        cl::arr<TextureData> m_depth_texture_data;
        u32 m_render_texture_generation = numeric::invalid_id;
    };

    namespace vk {
        [[nodiscard]] result<swapchain_outcome, renderer_error>
        classify_swapchain_result(
            VkResult result,
            renderer_error_code failure_code);
    }
}
