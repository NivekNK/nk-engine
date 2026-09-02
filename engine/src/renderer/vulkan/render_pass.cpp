#include "nkpch.h"

#include "vulkan/render_pass.h"

#include "vulkan/swapchain.h"
#include "vulkan/device.h"
#include "vulkan/command_buffer.h"
#include "vulkan/framebuffer.h"
#include "glm/vec4.hpp"

namespace nk {
    result<void, renderer_error> RenderPass::init(
        const RenderPassCreateInfo& create_info,
        Swapchain& swapchain,
        Device* device,
        VkAllocationCallbacks* vulkan_allocator) {
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        m_render_area = create_info.render_area;
        m_clear_color = create_info.clear_color;
        m_depth = create_info.depth;
        m_stencil = create_info.stencil;
        m_clear_flags = create_info.clear_flags;

        // Main subpass
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkAttachmentDescription attachment_descriptions[2]{};
        m_attachment_count = create_info.has_depth_attachment ? 2 : 1;

        // Color attachment
        VkAttachmentDescription color_attachment = {};
        color_attachment.format = swapchain.get_image_format().format; // TODO: configurable
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        const bool clear_color = has_flag(
            create_info.clear_flags, RenderPassClearFlags::color);
        color_attachment.loadOp = clear_color
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : create_info.has_previous_pass
                ? VK_ATTACHMENT_LOAD_OP_LOAD
                : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout =
            create_info.has_previous_pass && !clear_color
                ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                : VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = create_info.has_next_pass
            ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        color_attachment.flags = 0;

        attachment_descriptions[0] = color_attachment;

        VkAttachmentReference color_attachment_reference = {};
        color_attachment_reference.attachment = 0; // Attachment description array index
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_reference;

        VkAttachmentDescription depth_attachment = {};
        depth_attachment.format = m_device->get_depth_format();
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = has_flag(
            create_info.clear_flags, RenderPassClearFlags::depth)
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : VK_ATTACHMENT_LOAD_OP_LOAD;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = has_flag(
            create_info.clear_flags, RenderPassClearFlags::stencil)
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachment_descriptions[1] = depth_attachment;

        // Depth attachment reference
        VkAttachmentReference depth_attachment_reference = {};
        depth_attachment_reference.attachment = 1;
        depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass.pDepthStencilAttachment = create_info.has_depth_attachment
            ? &depth_attachment_reference
            : nullptr;

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
        dependency.srcAccessMask = create_info.has_previous_pass
            ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            : 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            (create_info.has_depth_attachment
                ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                : 0);
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (create_info.has_depth_attachment)
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
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::render_pass_creation_failed,
                .native_code = static_cast<i32>(result),
            });
        TraceLog("nk::RenderPass initialized.");
        return ok();
    }

    void RenderPass::shutdown() {
        if (m_render_pass) {
            vkDestroyRenderPass(m_device->get(), m_render_pass, m_vulkan_allocator);
            m_render_pass = nullptr;
        }
        TraceLog("nk::RenderPass shutdown.");
    }

    void RenderPass::begin(CommandBuffer& command_buffer, Framebuffer& framebuffer) {
        VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin_info.renderPass = m_render_pass;
        begin_info.framebuffer = framebuffer;
        begin_info.renderArea = m_render_area;

        VkClearValue clear_values[2]{};
        clear_values[0].color.float32[0] = m_clear_color.r;
        clear_values[0].color.float32[1] = m_clear_color.g;
        clear_values[0].color.float32[2] = m_clear_color.b;
        clear_values[0].color.float32[3] = m_clear_color.a;
        clear_values[1].depthStencil.depth = m_depth;
        clear_values[1].depthStencil.stencil = m_stencil;

        begin_info.clearValueCount = m_attachment_count;
        begin_info.pClearValues = clear_values;

        vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
        command_buffer.set_state(CommandBufferState::InRenderPass);
    }

    void RenderPass::end(CommandBuffer& command_buffer) {
        vkCmdEndRenderPass(command_buffer);
        command_buffer.set_state(CommandBufferState::Recording);
    }
}
