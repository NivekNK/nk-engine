#include "nkpch.h"

#include "vulkan/renderer_backend.h"

#include "nk/window.h"
#include "memory/malloc_allocator.h"

namespace nk {
    RendererBackend::RendererBackend(Window& window, str application_name)
        : m_window{window} {
        auto allocator = new MallocAllocator();
        allocator->allocator_init("VulkanRenderer", MemoryType::Renderer);
        m_allocator = allocator;

        m_instance.init(application_name.c_str(), m_allocator, m_vulkan_allocator);
        m_device.init(m_window, m_instance, m_allocator, m_vulkan_allocator);
        m_swapchain.init(window.width(), window.height(), m_device, m_vulkan_allocator);

        auto main_render_pass_create_info = RenderPassCreateInfo {
            .render_area = {{0, 0}, {m_window.width(), m_window.height()}},
            .clear_color = {0.0f, 0.0f, 0.2f, 1.0f},
            .depth = 1.0f,
            .stencil = 0,
        };
        m_main_render_pass.init(main_render_pass_create_info, m_device, m_swapchain, m_vulkan_allocator);

        TraceLog("nk::RendererBackend created.");
    }

    RendererBackend::~RendererBackend() {
        m_main_render_pass.shutdown(m_device);

        m_swapchain.shutdown(m_device);

        m_device.shutdown(m_instance);

        m_instance.shutdown();

        delete m_allocator;
        TraceLog("nk::RendererBackend destroyed.");
    }

    bool RendererBackend::draw_frame(const RenderPacket& packet) {
        if (begin_frame(packet.delta_time)) {
            if (!end_frame(packet.delta_time)) {
                ErrorLog("Vulkan RendererBackend::draw_frame failed!");
                return false;
            }
        }

        return true;
    }

    bool RendererBackend::begin_frame(f64 delta_time) {
        return true;
    }

    bool RendererBackend::end_frame(f64 delta_time) {
        m_frame_number++;
        return true;
    }
}