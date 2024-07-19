#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "vulkan/command_buffer.h"
#include "vulkan/framebuffer.h"
#include "core/dyarr.h"

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

        void create_command_buffers();
        void regenerate_framebuffers(const u16 width, const u16 height);

        Window& m_window;
        Allocator* m_allocator;

        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        u32 m_frame_number = 0;

        Instance m_instance;
        Device m_device;
        Swapchain m_swapchain;
        RenderPass m_main_render_pass;

        Dyarr<CommandBuffer> m_graphics_command_buffers;

        Dyarr<Framebuffer> m_framebuffers;
    };
}