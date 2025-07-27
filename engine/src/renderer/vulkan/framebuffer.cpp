#include "nkpch.h"

#include "vulkan/framebuffer.h"

#include "vulkan/device.h"
#include "vulkan/render_pass.h"

namespace nk {
    Framebuffer::Framebuffer(Framebuffer&& other)
        : m_device{other.m_device},
          m_allocator{other.m_allocator},
          m_vulkan_allocator{other.m_vulkan_allocator},
          m_framebuffer{other.m_framebuffer},
          m_width{other.m_width},
          m_height{other.m_height},
          m_attachments{std::move(other.m_attachments)} {
        other.m_vulkan_allocator = nullptr;
        other.m_device = nullptr;
        other.m_allocator = nullptr;
        other.m_framebuffer = nullptr;
        other.m_width = 0;
        other.m_height = 0;
        other.m_attachments.arr_reset();
    }

    Framebuffer& Framebuffer::operator=(Framebuffer&& other) {
        m_vulkan_allocator = other.m_vulkan_allocator;
        m_device = other.m_device;
        m_allocator = other.m_allocator;
        m_framebuffer = other.m_framebuffer;
        m_width = other.m_width;
        m_height = other.m_height;
        m_attachments = std::move(other.m_attachments);

        other.m_vulkan_allocator = nullptr;
        other.m_device = nullptr;
        other.m_allocator = nullptr;
        other.m_framebuffer = nullptr;
        other.m_width = 0;
        other.m_height = 0;
        other.m_attachments.arr_reset();

        return *this;
    }

    void Framebuffer::init(const u32 width,
                           const u32 height,
                           cl::arr<VkImageView>& attachments,
                           Device* device,
                           RenderPass& render_pass,
                           VkAllocationCallbacks* vulkan_allocator) {
        m_width = width;
        m_height = height;
        m_attachments = std::move(attachments);
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        // Creation info
        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount = m_attachments.length();
        framebuffer_create_info.pAttachments = m_attachments.data();
        framebuffer_create_info.width = width;
        framebuffer_create_info.height = height;
        framebuffer_create_info.layers = 1;

        VulkanCheck(vkCreateFramebuffer(m_device->get(), &framebuffer_create_info, m_vulkan_allocator, &m_framebuffer));
    }

    void Framebuffer::shutdown() {
        if (m_framebuffer != nullptr) {
            vkDestroyFramebuffer(m_device->get(), m_framebuffer, m_vulkan_allocator);
            m_framebuffer = nullptr;
        }

        if (!m_attachments.empty()) {
            m_attachments.arr_shutdown();
        }
    }

    void Framebuffer::renew(const u32 width,
                            const u32 height,
                            cl::arr<VkImageView>& attachments,
                            Device* device,
                            RenderPass& render_pass,
                            VkAllocationCallbacks* vulkan_allocator) {
        shutdown();
        init(width, height, attachments, device, render_pass, vulkan_allocator);
    }
}
