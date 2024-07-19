#include "nkpch.h"

#include "vulkan/framebuffer.h"

#include "vulkan/device.h"
#include "vulkan/render_pass.h"

namespace nk {
    Framebuffer::Framebuffer(Framebuffer&& other) {
        m_allocator = other.m_allocator;
        m_device = other.m_device;
        m_framebuffer = other.m_framebuffer;

        other.m_allocator = nullptr;
        other.m_device = nullptr;
        other.m_framebuffer = nullptr;

        m_attachments = std::move(other.m_attachments);
    }

    Framebuffer& Framebuffer::operator=(Framebuffer&& other) {
        m_allocator = other.m_allocator;
        m_device = other.m_device;
        m_framebuffer = other.m_framebuffer;

        other.m_allocator = nullptr;
        other.m_device = nullptr;
        other.m_framebuffer = nullptr;

        m_attachments = std::move(other.m_attachments);

        return *this;
    }

    void Framebuffer::init(const u16 width,
                           const u16 height,
                           Arr<VkImageView>& attachments,
                           Device* device,
                           RenderPass& render_pass,
                           VkAllocationCallbacks* allocator) {
        m_device = device;
        m_allocator = allocator;
        m_attachments = std::move(attachments);

        // Creation info
        VkFramebufferCreateInfo framebuffer_create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount = m_attachments.length();
        framebuffer_create_info.pAttachments = m_attachments.data();
        framebuffer_create_info.width = width;
        framebuffer_create_info.height = height;
        framebuffer_create_info.layers = 1;

        VulkanCheck(vkCreateFramebuffer(m_device->get(), &framebuffer_create_info, m_allocator, &m_framebuffer));
    }

    void Framebuffer::shutdown() {
        if (m_framebuffer == nullptr)
            return;

        vkDestroyFramebuffer(m_device->get(), m_framebuffer, m_allocator);
        m_attachments.clear();
        m_framebuffer = nullptr;
    }
}
