#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"

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

        Window& m_window;
        u32 m_frame_number;

        Allocator* m_allocator;

        VkAllocationCallbacks* m_vulkan_allocator;
        Instance m_instance;
    };
}
