#include "nkpch.h"

#include "vulkan/framebuffer.h"

#include "vulkan/device.h"
#include "vulkan/render_pass.h"
#include "vulkan/resources/texture_data.h"

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

        return *this;
    }

    result<void, renderer_error> Framebuffer::init(
        const RenderTarget& target,
        Device* device,
        RenderPass& render_pass,
        VkAllocationCallbacks* vulkan_allocator) {
        if (m_framebuffer != VK_NULL_HANDLE || device == nullptr ||
            !target.current() || target.attachment_count() == 0) {
            return err(renderer_error{
                renderer_error_code::render_target_config_invalid, 0});
        }
        if (!m_attachments.arr_init(
                device->allocator(), target.attachment_count())) {
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }
        for (u8 index = 0; index < target.attachment_count(); ++index) {
            const Texture* texture = target.attachment(index);
            const TextureData* data = texture == nullptr
                ? nullptr
                : static_cast<const TextureData*>(texture->m_internal_data);
            if (data == nullptr || data->image.get_view() == VK_NULL_HANDLE) {
                shutdown();
                return err(renderer_error{
                    renderer_error_code::render_target_config_invalid, 0});
            }
            m_attachments[index] = data->image.get_view();
        }
        m_width = target.width();
        m_height = target.height();
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        // Creation info
        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount = m_attachments.length();
        framebuffer_create_info.pAttachments = m_attachments.data();
        framebuffer_create_info.width = m_width;
        framebuffer_create_info.height = m_height;
        framebuffer_create_info.layers = 1;

        const VkResult result = vkCreateFramebuffer(
            m_device->get(),
            &framebuffer_create_info,
            m_vulkan_allocator,
            &m_framebuffer);
        if (result != VK_SUCCESS) {
            shutdown();
            return err(renderer_error{
                .code = renderer_error_code::framebuffer_creation_failed,
                .native_code = static_cast<i32>(result),
            });
        }
        return ok();
    }

    void Framebuffer::shutdown() {
        if (m_framebuffer != nullptr) {
            vkDestroyFramebuffer(m_device->get(), m_framebuffer, m_vulkan_allocator);
            m_framebuffer = nullptr;
        }
        m_attachments.arr_shutdown();
    }

}
