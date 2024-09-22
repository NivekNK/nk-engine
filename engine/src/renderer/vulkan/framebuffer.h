#pragma once

#include "vulkan/vk.h"
#include "core/arr.h"

namespace nk {
    class Device;
    class RenderPass;

    class Framebuffer {
    public:
        Framebuffer() = default;
        ~Framebuffer() { shutdown(); }

        Framebuffer(const Framebuffer&) = delete;
        Framebuffer& operator=(const Framebuffer&) = delete;

        Framebuffer(Framebuffer&& other);
        Framebuffer& operator=(Framebuffer&& other);

        void init(const u16 width,
                  const u16 height,
                  Arr<VkImageView>& attachments,
                  Device* device,
                  RenderPass& render_pass,
                  VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        void renew(const u16 width,
                   const u16 height,
                   Arr<VkImageView>& attachments,
                   Device* device,
                   RenderPass& render_pass,
                   VkAllocationCallbacks* vulkan_allocator);

        VkFramebuffer get() { return m_framebuffer; }
        VkFramebuffer operator()() { return m_framebuffer; }
        operator VkFramebuffer() { return m_framebuffer; }

    private:
        Device* m_device;
        VkAllocationCallbacks* m_vulkan_allocator;

        VkFramebuffer m_framebuffer;
        Arr<VkImageView> m_attachments;
    };
}
