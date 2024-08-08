#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "vulkan/command_buffer.h"
#include "vulkan/framebuffer.h"
#include "vulkan/fence.h"
#include "core/dyarr.h"
#include "core/arr.h"

namespace nk {
    class Window;
    class Allocator;

    class RendererBackend : public Renderer {
    public:
        RendererBackend(Window& window, str application_name);
        virtual ~RendererBackend() override;

        virtual bool draw_frame(const RenderPacket& packet) override;

    private:
        bool begin_frame(f64 delta_time);
        bool end_frame(f64 delta_time);

        void on_window_resize(u16 width, u16 height);

        void regenerate_framebuffers(const u16 width, const u16 height);
        void create_command_buffers();
        void create_sync_objects();

        bool recreate_swapchain();

        Window& m_window;
        Allocator* m_allocator;

        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        u32 m_frame_number = 0;

        Instance m_instance;
        Device m_device;
        Swapchain m_swapchain;
        RenderPass m_main_render_pass;

        Dyarr<Framebuffer> m_framebuffers;

        Dyarr<CommandBuffer> m_graphics_command_buffers;

        Arr<VkSemaphore> m_image_available_semaphores;
        Arr<VkSemaphore> m_queue_complete_semaphores;
        Arr<Fence> m_in_flight_fences;
        Arr<Fence> m_images_in_flight;

        bool m_recreating_swapchain;

        bool m_window_was_resized;
        u16 m_width;
        u16 m_height;

        u32 m_image_index;
    };
}