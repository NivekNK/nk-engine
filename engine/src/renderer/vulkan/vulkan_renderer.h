#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "collections/dyarr.h"
#include "vulkan/framebuffer.h"
#include "vulkan/command_buffer.h"
#include "vulkan/fence.h"

namespace nk {
    class VulkanRenderer : public Renderer {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() = default;

        virtual void on_resized(u32 width, u32 height) override;

        virtual void init() override;
        virtual void shutdown() override;
        virtual bool begin_frame(f64 delta_time) override;
        virtual bool end_frame(f64 delta_time) override;

    private:
        void recreate_framebuffers();
        void recreate_command_buffers();
        void recreate_sync_objects();
        void recreate_swapchain();

        VkAllocationCallbacks* m_vulkan_allocator;
        Instance m_instance;
        Device m_device;
        Swapchain m_swapchain;
        RenderPass m_main_render_pass;
        cl::dyarr<Framebuffer> m_framebuffers;
        cl::dyarr<CommandBuffer> m_graphics_command_buffers;

        // Per-frame semaphores for synchronization
        cl::dyarr<VkSemaphore> m_image_available_semaphores;
        cl::dyarr<VkSemaphore> m_queue_complete_semaphores;
        cl::dyarr<Fence> m_in_flight_fences;
        cl::dyarr<Fence*> m_images_in_flight;

        u32 m_framebuffer_width;
        u32 m_framebuffer_height;
        u32 m_cached_framebuffer_width;
        u32 m_cached_framebuffer_height;

        // Current genration of frambuffer size. If it does not match m_framebuffer_last_generation
        // a new one should be generated.
        u64 m_framebuffer_size_generation;

        // The generation of the framebuffer when it was last created. Set to m_framebuffer_size_generation
        // when updated.
        u64 m_framebuffer_last_generation;

        u32 m_image_index;
        u32 m_current_frame;
    };
}
