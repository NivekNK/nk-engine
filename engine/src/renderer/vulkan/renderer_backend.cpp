#include "nkpch.h"

#include "vulkan/renderer_backend.h"

#include "nk/window.h"
#include "memory/malloc_allocator.h"

namespace nk {
    RendererBackend::RendererBackend(Window& window, cstr application_name)
        : m_window{window},
          m_frame_number{0} {
        auto allocator = new MallocAllocator();
        allocator->allocator_init("VulkanRenderer", MemoryType::Renderer);
        m_allocator = allocator;

        m_vulkan_allocator = nullptr;

        m_instance.init(application_name, m_allocator, m_vulkan_allocator);
        m_device.init(m_window, &m_instance, m_allocator, m_vulkan_allocator);
        m_swapchain.init(m_framebuffer_width, m_framebuffer_height, &m_device, m_allocator, m_vulkan_allocator);

        RenderPassCreateInfo main_render_pass_create_info {
            .render_area = {{0, 0}, {m_framebuffer_width, m_framebuffer_height}},
            .clear_color = {1.0f, 0.0f, 0.45f, 1.0f},
            .depth = 1.0f,
            .stencil = 0,
        };
        m_main_render_pass.init(main_render_pass_create_info, m_device, m_swapchain, m_vulkan_allocator);

        recreate_command_buffers();
        InfoLog("Vulkan Command Buffers created.");

        TraceLog("nk::RendererBackend created.");
    }

    RendererBackend::~RendererBackend() {
        m_graphics_command_buffers.clear();
        InfoLog("Vulkan Command Buffers freed.");

        m_main_render_pass.shutdown(m_device);
        m_swapchain.shutdown();
        m_device.shutdown();
        m_instance.shutdown();

        m_vulkan_allocator = nullptr;

        delete m_allocator;
        TraceLog("nk::RendererBackend destroyed.");
    }

    bool RendererBackend::draw_frame(const RenderPacket& packet) {
        if (begin_frame(packet.delta_time)) {
            if (!end_frame(packet.delta_time)) {
                DebugLog("RendererBackend::end_frame failed. Shutting down application.");
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

    void RendererBackend::recreate_command_buffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (image_count > 0) {
            if (m_graphics_command_buffers.length() > 0) {
                if (m_graphics_command_buffers.length() != image_count)
                    m_graphics_command_buffers.resize(image_count);
            } else {
                m_graphics_command_buffers.init(m_allocator, image_count, image_count);
            }

            for (auto& command_buffer : m_graphics_command_buffers) {
                command_buffer.renew(m_device.get_graphics_command_pool(), &m_device, true, false);
            }
        } else {
            m_graphics_command_buffers.reset();
        }
    }
}
