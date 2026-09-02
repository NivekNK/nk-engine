#pragma once

#include "vulkan/vk.h"
#include "collections/arr.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

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

        [[nodiscard]] result<void, renderer_error> init(
            u32 width,
            u32 height,
            cl::arr<VkImageView>& attachments,
            Device* device,
            RenderPass& render_pass,
            VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        [[nodiscard]] result<void, renderer_error> renew(
            u32 width,
            u32 height,
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
        Device* m_device = nullptr;
        mem::Allocator* m_allocator = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;

        VkFramebuffer m_framebuffer = nullptr;
        u32 m_width = 0;
        u32 m_height = 0;
        cl::arr<VkImageView> m_attachments;
    };
}
