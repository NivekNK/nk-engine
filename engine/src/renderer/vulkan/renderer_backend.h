#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "vulkan/command_buffer.h"

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

        void on_window_resize(u16 width, u16 height) {}

        void recreate_command_buffers();

        Window& m_window;
        u32 m_frame_number;

        Allocator* m_allocator;

        VkAllocationCallbacks* m_vulkan_allocator;
        Instance m_instance;
        Device m_device;

        u16 m_framebuffer_width;
        u16 m_framebuffer_height;
        u32 m_image_index;
        bool m_recreating_swapchain;
        Swapchain m_swapchain;

        RenderPass m_main_render_pass;

        Dyarr<CommandBuffer> m_graphics_command_buffers;
    };
}
