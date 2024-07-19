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

        regenerate_framebuffers(m_window.width(), m_window.height());
        InfoLog("Vulkan Framebuffers created.");

        create_command_buffers();
        InfoLog("Vulkan Command Buffers created.");

        TraceLog("nk::RendererBackend created.");
    }

    RendererBackend::~RendererBackend() {
        m_graphics_command_buffers.free();
        InfoLog("Vulkan Command Buffers destroyed.");

        m_framebuffers.free();
        InfoLog("Vulkan Framebuffers destroyed.");

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

    void RendererBackend::create_command_buffers() {
        const u32 image_count = m_swapchain.get_image_count();

        if (m_graphics_command_buffers.empty()) {
            m_graphics_command_buffers.init(m_allocator, image_count);

            for (auto& command_buffer : m_graphics_command_buffers) {
                command_buffer.init(m_device.get_graphics_command_pool(), &m_device, true);
            }
        } else {
            const u32 old_image_count = m_graphics_command_buffers.length();

            if (image_count > old_image_count) {
                m_graphics_command_buffers.grow(image_count);
            }

            u32 i = 0;
            for (auto& command_buffer : m_graphics_command_buffers) {
                if (i < old_image_count) {
                    command_buffer.shutdown();
                }
                command_buffer.init(m_device.get_graphics_command_pool(), &m_device, true);
                i++;
            }
        }
    }

    void RendererBackend::regenerate_framebuffers(const u16 width, const u16 height) {
        const u32 image_count = m_swapchain.get_image_count();

        if (m_framebuffers.empty()) {
            m_framebuffers.init(m_allocator, image_count);

            u32 i = 0;
            for (auto& framebuffer : m_framebuffers) {
                // TODO: make this dynamic based on the currently configured attachments
                Arr<VkImageView> attachments;
                attachments.init_list(m_allocator, {m_swapchain.get_image_view_at(i), m_swapchain.get_depth_attachment().get_view()});
                framebuffer.init(width, height, attachments, &m_device, m_main_render_pass, m_vulkan_allocator);
                i++;
            }
        } else {
            const u32 old_image_count = m_framebuffers.length();

            if (image_count > old_image_count) {
                m_framebuffers.grow(image_count);
            }

            u32 i = 0;
            for (auto& framebuffer : m_framebuffers) {
                if (i < old_image_count) {
                    framebuffer.shutdown();
                }
                // TODO: make this dynamic based on the currently configured attachments
                Arr<VkImageView> attachments;
                attachments.init_list(m_allocator, {m_swapchain.get_image_view_at(i), m_swapchain.get_depth_attachment().get_view()});
                framebuffer.init(width, height, attachments, &m_device, m_main_render_pass, m_vulkan_allocator);
                i++;
            }
        }
    }
}