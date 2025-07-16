#include "nkpch.h"

#include "vulkan/vulkan_renderer.h"

namespace nk {
    void VulkanRenderer::on_resized(u32 width, u32 height) {
    }

    void VulkanRenderer::init() {
        m_vulkan_allocator = nullptr;

        m_instance.init(m_application_name.c_str(), m_allocator, m_vulkan_allocator);
        m_device.init(m_platform, &m_instance, m_allocator, m_vulkan_allocator);
    }

    void VulkanRenderer::shutdown() {
        m_device.shutdown();
        m_instance.shutdown();
    }

    bool VulkanRenderer::begin_frame(f64 delta_time) {
        return true;
    }

    bool VulkanRenderer::end_frame(f64 delta_time) {
        return true;
    }
}
