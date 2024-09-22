#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "vulkan/framebuffer.h"
#include "vulkan/command_buffer.h"
#include "vulkan/fence.h"

namespace nk {
    class Window;
    class Allocator;

    class RendererBackend : public Renderer {
    public:
        RendererBackend(Window& window, cstr application_name);
        virtual ~RendererBackend() override;

        virtual bool draw_frame(const RenderPacket& packet) override;

    private:
        bool begin_frame(f64 delta_time);
        bool end_frame(f64 delta_time);

        void on_window_resize(u16 width, u16 height);

        void recreate_framebuffers();
        void recreate_command_buffers();
        void create_sync_objects();
        bool recreate_swapchain();

        Window& m_window;
        u32 m_frame_number;

        Allocator* m_allocator;

        VkAllocationCallbacks* m_vulkan_allocator;
        Instance m_instance;
        Device m_device;

        u16 m_framebuffer_width;
        u16 m_framebuffer_height;
        u16 m_cached_framebuffer_width;
        u16 m_cached_framebuffer_height;

        // Current genration of frambuffer size. If it does not match m_framebuffer_last_generation
        // a new one should be generated.
        u64 m_framebuffer_size_generation = 0;

        // The generation of the framebuffer when it was last created. Set to m_framebuffer_size_generation
        // when updated.
        u64 m_framebuffer_last_generation = 0;

        u32 m_image_index;
        bool m_recreating_swapchain;
        Swapchain m_swapchain;

        RenderPass m_main_render_pass;

        Dyarr<Framebuffer> m_framebuffers;

        Dyarr<CommandBuffer> m_graphics_command_buffers;

        Arr<VkSemaphore> m_image_available_semaphores;
        Arr<VkSemaphore> m_queue_complete_semaphores;
        Arr<Fence> m_in_flight_fences;
        Arr<Fence*> m_images_in_flight;
    };
}
