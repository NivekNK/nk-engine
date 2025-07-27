#pragma once

#include "vulkan/vk.h"
#include "collections/arr.h"

namespace nk {
    namespace mem {
        class Allocator;
    }

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

        void init(const u32 width,
                  const u32 height,
                  cl::arr<VkImageView>& attachments,
                  Device* device,
                  RenderPass& render_pass,
                  VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        void renew(const u32 width,
                   const u32 height,
                   cl::arr<VkImageView>& attachments,
                   Device* device,
                   RenderPass& render_pass,
                   VkAllocationCallbacks* vulkan_allocator);

        VkFramebuffer get() { return m_framebuffer; }
        VkFramebuffer operator()() { return m_framebuffer; }
        operator VkFramebuffer() { return m_framebuffer; }

        u32 get_width() const { return m_width; }
        u32 get_height() const { return m_height; }

    private:
        Device* m_device;
        mem::Allocator* m_allocator;
        VkAllocationCallbacks* m_vulkan_allocator;

        VkFramebuffer m_framebuffer;
        u32 m_width;
        u32 m_height;
        cl::arr<VkImageView> m_attachments;
    };
}
