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
    }

    RendererBackend::~RendererBackend() {
        m_device.shutdown();
        m_instance.shutdown();

        m_vulkan_allocator = nullptr;

        delete m_allocator;
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
}
