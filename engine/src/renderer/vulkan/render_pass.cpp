#include "nkpch.h"

#include "vulkan/render_pass.h"

#include "vulkan/device.h"
#include "vulkan/command_buffer.h"
#include "vulkan/framebuffer.h"
#include "vulkan/texture_format.h"
#include "glm/vec4.hpp"

namespace nk {
    result<void, renderer_error> RenderPass::init(
        const RenderPassConfig& config,
        Device* device,
        VkAllocationCallbacks* vulkan_allocator) {
        if (m_device != nullptr || device == nullptr ||
            device->get() == VK_NULL_HANDLE ||
            !validate_render_pass_config(config)) {
            return err(renderer_error{
                renderer_error_code::render_target_config_invalid, 0});
        }
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        m_render_area = {
            {config.area.x, config.area.y},
            {config.area.width, config.area.height},
        };
        m_clear_color = config.clear_color;
        m_depth = config.clear_depth;
        m_stencil = config.clear_stencil;
        auto signature = render_pass_signature(config);
        if (!signature) {
            m_device = nullptr;
            m_vulkan_allocator = nullptr;
            return err(renderer_error{
                renderer_error_code::render_target_config_invalid,
                static_cast<i32>(signature.error()),
            });
        }
        m_signature = *signature;
        m_attachment_count = static_cast<u32>(config.attachments.length());
        for (u32 index = 0; index < m_attachment_count; ++index) {
            m_attachments[index] = config.attachments[index];
            if (m_attachments[index].role == RenderAttachmentRole::color)
                m_color_format = vk::texture_format(m_attachments[index].format);
            else
                m_depth_format = vk::texture_format(m_attachments[index].format);
        }
        // Modern pipelines need only attachment formats. No VkRenderPass or
        // framebuffer object is created on the dynamic-rendering path.
        if (device->dynamic_rendering())
            return ok();

        // Main subpass
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkAttachmentDescription attachment_descriptions[
            max_render_target_attachments]{};
        VkAttachmentReference color_reference{};
        VkAttachmentReference depth_reference{};
        bool has_depth = false;
        for (u32 index = 0; index < m_attachment_count; ++index) {
            const RenderAttachmentConfig& attachment = m_attachments[index];
            VkAttachmentDescription& description = attachment_descriptions[index];
            description.format = vk::texture_format(attachment.format);
            description.samples = vk::sample_count(attachment.sample_count);
            description.loadOp = attachment.load == RenderLoadOperation::clear
                ? VK_ATTACHMENT_LOAD_OP_CLEAR
                : attachment.load == RenderLoadOperation::load
                    ? VK_ATTACHMENT_LOAD_OP_LOAD
                    : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.storeOp = attachment.store == RenderStoreOperation::store
                ? VK_ATTACHMENT_STORE_OP_STORE
                : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            description.stencilLoadOp = description.loadOp;
            description.stencilStoreOp = description.storeOp;
            description.initialLayout =
                attachment.load == RenderLoadOperation::load
                    ? attachment.role == RenderAttachmentRole::color
                        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                        : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    : VK_IMAGE_LAYOUT_UNDEFINED;
            switch (attachment.final_use) {
                case RenderAttachmentUse::color_attachment:
                    description.finalLayout =
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    break;
                case RenderAttachmentUse::depth_stencil_attachment:
                    description.finalLayout =
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    break;
                case RenderAttachmentUse::present:
                    description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    break;
                case RenderAttachmentUse::sampled:
                    description.finalLayout =
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    break;
                case RenderAttachmentUse::transfer_source:
                    description.finalLayout =
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    break;
            }

            if (attachment.role == RenderAttachmentRole::color) {
                color_reference = {
                    index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            } else {
                depth_reference = {
                    index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
                has_depth = true;
            }
        }

        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_reference;
        subpass.pDepthStencilAttachment = has_depth ? &depth_reference : nullptr;

        // Input from a shader
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;

        // Attachments used for multisampling colour attachments
        subpass.pResolveAttachments = nullptr;

        // Attachments not used in this subpass, but must be preserved for the next.
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;

        // Render pass dependencies. TODO: make this configurable.
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = config.has_previous_pass
            ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            : 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            (has_depth
                ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                : 0);
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (has_depth)
            dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Render pass create.
        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = m_attachment_count;
        render_pass_create_info.pAttachments = attachment_descriptions;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies = &dependency;
        render_pass_create_info.pNext = nullptr;
        render_pass_create_info.flags = 0;

        const VkResult result = vkCreateRenderPass(
            m_device->get(),
            &render_pass_create_info,
            m_vulkan_allocator,
            &m_render_pass);
        if (result != VK_SUCCESS) {
            m_device = nullptr;
            m_vulkan_allocator = nullptr;
            m_color_format = VK_FORMAT_UNDEFINED;
            m_depth_format = VK_FORMAT_UNDEFINED;
            m_signature = {};
            m_attachment_count = 0;
            return err(renderer_error{
                .code = renderer_error_code::render_pass_creation_failed,
                .native_code = static_cast<i32>(result),
            });
        }
        TraceLog("nk::RenderPass initialized.");
        return ok();
    }

    void RenderPass::shutdown() {
        if (m_render_pass) {
            vkDestroyRenderPass(m_device->get(), m_render_pass, m_vulkan_allocator);
            m_render_pass = nullptr;
        }
        m_device = nullptr;
        m_vulkan_allocator = nullptr;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_signature = {};
        m_attachment_count = 0;
        TraceLog("nk::RenderPass shutdown.");
    }

    void RenderPass::begin(CommandBuffer& command_buffer, Framebuffer& framebuffer) {
        VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin_info.renderPass = m_render_pass;
        begin_info.framebuffer = framebuffer;
        begin_info.renderArea = m_render_area;

        VkClearValue clear_values[max_render_target_attachments]{};
        for (u32 index = 0; index < m_attachment_count; ++index) {
            if (m_attachments[index].role == RenderAttachmentRole::color) {
                clear_values[index].color.float32[0] = m_clear_color.r;
                clear_values[index].color.float32[1] = m_clear_color.g;
                clear_values[index].color.float32[2] = m_clear_color.b;
                clear_values[index].color.float32[3] = m_clear_color.a;
            } else {
                clear_values[index].depthStencil.depth = m_depth;
                clear_values[index].depthStencil.stencil = m_stencil;
            }
        }

        begin_info.clearValueCount = m_attachment_count;
        begin_info.pClearValues = clear_values;

        vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        command_buffer.set_state(CommandBufferState::InRenderPass);
    }

    void RenderPass::end(CommandBuffer& command_buffer) {
        vkCmdEndRenderPass(command_buffer);
        command_buffer.set_state(CommandBufferState::Recording);
    }

    const RenderAttachmentConfig* RenderPass::attachment(
        const RenderAttachmentRole role) const noexcept {
        for (u32 index = 0; index < m_attachment_count; ++index)
            if (m_attachments[index].role == role)
                return &m_attachments[index];
        return nullptr;
    }
}
