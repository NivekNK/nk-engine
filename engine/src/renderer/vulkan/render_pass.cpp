#include "nkpch.h"

#include "vulkan/render_pass.h"

namespace nk {
    void RenderPass::init(const RenderPassCreateInfo& create_info,
                          Device& device,
                          Swapchain& swapchain,
                          VkAllocationCallbacks* allocator) {
        m_allocator = allocator;

        // Main subpass
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        // Attachments
        //TODO: make this configurable.
        constexpr u32 attachment_description_count = 2;
        VkAttachmentDescription attachment_descriptions[attachment_description_count];

        // Color attachment
        VkAttachmentDescription color_attachment = {};
        color_attachment.format = swapchain.get_image_format().format; // TODO: configurable
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;     // Do not expect any particular layout before render pass starts.
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Transitioned to after the render pass
        color_attachment.flags = 0;

        attachment_descriptions[0] = color_attachment;

        VkAttachmentReference color_attachment_reference = {};
        color_attachment_reference.attachment = 0; // Attachment description array index
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_reference;

        // Depth attachment, if there is one
        VkAttachmentDescription depth_attachment = {};
        depth_attachment.format = device.get_depth_format();
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachment_descriptions[1] = depth_attachment;

        // Depth attachment reference
        VkAttachmentReference depth_attachment_reference = {};
        depth_attachment_reference.attachment = 1;
        depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // TODO: other attachment types (input, resolve, preserve)

        // Depth stencil data.
        subpass.pDepthStencilAttachment = &depth_attachment_reference;

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
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = 0;

        // Render pass create.
        VkRenderPassCreateInfo render_pass_create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        render_pass_create_info.attachmentCount = attachment_description_count;
        render_pass_create_info.pAttachments = attachment_descriptions;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies = &dependency;
        render_pass_create_info.pNext = nullptr;
        render_pass_create_info.flags = 0;

        VulkanCheck(vkCreateRenderPass(device, &render_pass_create_info, m_allocator, &m_render_pass));
        TraceLog("nk::RenderPass initialized.");
    }

    void RenderPass::shutdown(Device& device) {
        if (m_render_pass) {
            vkDestroyRenderPass(device, m_render_pass, m_allocator);
        }
        TraceLog("nk::RenderPass shutdown.");
    }
}
