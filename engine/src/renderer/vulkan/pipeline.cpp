#include "nkpch.h"

#include "vulkan/pipeline.h"

#include "vulkan/device.h"
#include "vulkan/render_pass.h"
#include "vulkan/command_buffer.h"
#include "vulkan/utils.h"

#include <glm/vertex_3d.h>
#include <glm/ext/matrix_float4x4.hpp>

namespace nk {
    void Pipeline::init(const PipelineCreateInfo& create_info) {
        m_device = create_info.device;
        m_vulkan_allocator = create_info.vulkan_allocator;

        // Viewport state
        VkPipelineViewportStateCreateInfo viewport_state_create_info;
        memset(&viewport_state_create_info, 0, sizeof(viewport_state_create_info));
        viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_create_info.viewportCount = 1;
        viewport_state_create_info.pViewports = &create_info.viewport;
        viewport_state_create_info.scissorCount = 1;
        viewport_state_create_info.pScissors = &create_info.scissor;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer_create_info;
        memset(&rasterizer_create_info, 0, sizeof(rasterizer_create_info));
        rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer_create_info.depthClampEnable = VK_FALSE;
        rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
        rasterizer_create_info.polygonMode = create_info.is_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        rasterizer_create_info.lineWidth = 1.0f;
        rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer_create_info.depthBiasEnable = VK_FALSE;
        rasterizer_create_info.depthBiasConstantFactor = 0.0f;
        rasterizer_create_info.depthBiasClamp = 0.0f;
        rasterizer_create_info.depthBiasSlopeFactor = 0.0f;

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisample_create_info;
        memset(&multisample_create_info, 0, sizeof(multisample_create_info));
        multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_create_info.sampleShadingEnable = VK_FALSE;
        multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_create_info.minSampleShading = 1.0f;
        multisample_create_info.pSampleMask = nullptr;
        multisample_create_info.alphaToCoverageEnable = VK_FALSE;
        multisample_create_info.alphaToOneEnable = VK_FALSE;

        // Depth and stencil testing
        VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info;
        memset(&depth_stencil_create_info, 0, sizeof(depth_stencil_create_info));
        depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_create_info.depthTestEnable = VK_TRUE;
        depth_stencil_create_info.depthWriteEnable = VK_TRUE;
        depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_create_info.stencilTestEnable = VK_FALSE;

        // Color blend attachment
        VkPipelineColorBlendAttachmentState color_blend_attachment_create_info;
        memset(&color_blend_attachment_create_info, 0, sizeof(color_blend_attachment_create_info));
        color_blend_attachment_create_info.blendEnable = VK_TRUE;
        color_blend_attachment_create_info.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_create_info.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_create_info.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_create_info.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment_create_info.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment_create_info.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_create_info.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                            VK_COLOR_COMPONENT_G_BIT |
                                                            VK_COLOR_COMPONENT_B_BIT |
                                                            VK_COLOR_COMPONENT_A_BIT;

        // Color blend state
        VkPipelineColorBlendStateCreateInfo color_blend_create_info;
        memset(&color_blend_create_info, 0, sizeof(color_blend_create_info));
        color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_create_info.logicOpEnable = VK_FALSE;
        color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_create_info.attachmentCount = 1;
        color_blend_create_info.pAttachments = &color_blend_attachment_create_info;

        // Dynamic state
        constexpr u32 dynamic_state_count = 3;
        VkDynamicState dynamic_states[dynamic_state_count] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_LINE_WIDTH,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_create_info;
        memset(&dynamic_state_create_info, 0, sizeof(dynamic_state_create_info));
        dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_create_info.dynamicStateCount = dynamic_state_count;
        dynamic_state_create_info.pDynamicStates = dynamic_states;

        // Vertex input
        VkVertexInputBindingDescription vertex_input_binding_create_info;
        memset(&vertex_input_binding_create_info, 0, sizeof(vertex_input_binding_create_info));
        vertex_input_binding_create_info.binding = 0; // Binding index
        vertex_input_binding_create_info.stride = sizeof(glm::Vertex3D);
        vertex_input_binding_create_info.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Attributes
        VkPipelineVertexInputStateCreateInfo vertex_input_create_info;
        memset(&vertex_input_create_info, 0, sizeof(vertex_input_create_info));
        vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_create_info.vertexBindingDescriptionCount = 1;
        vertex_input_create_info.pVertexBindingDescriptions = &vertex_input_binding_create_info;
        vertex_input_create_info.vertexAttributeDescriptionCount = create_info.attribute_count;
        vertex_input_create_info.pVertexAttributeDescriptions = create_info.attributes;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info;
        memset(&input_assembly_create_info, 0, sizeof(input_assembly_create_info));
        input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipeline_layout_create_info;
        memset(&pipeline_layout_create_info, 0, sizeof(pipeline_layout_create_info));
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        // Push constants
        VkPushConstantRange push_constant;
        push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant.offset = sizeof(glm::mat4) * 0;
        push_constant.size = sizeof(glm::mat4) * 2;
        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges = &push_constant;

        // Descriptor set layouts
        pipeline_layout_create_info.setLayoutCount = create_info.descriptor_set_layout_count;
        pipeline_layout_create_info.pSetLayouts = create_info.descriptor_set_layouts;

        DebugLog("Creating pipeline layout with {} descriptor set layouts.", create_info.descriptor_set_layout_count);

        VulkanCheck(vkCreatePipelineLayout(
            create_info.device->get(),
            &pipeline_layout_create_info,
            create_info.vulkan_allocator,
            &m_layout
        ));

        DebugLog("Pipeline layout created.");

        // Pipeline create
        VkGraphicsPipelineCreateInfo pipeline_create_info;
        memset(&pipeline_create_info, 0, sizeof(pipeline_create_info));
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = create_info.stage_count;
        pipeline_create_info.pStages = create_info.stages;
        pipeline_create_info.pVertexInputState = &vertex_input_create_info;
        pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
        pipeline_create_info.pViewportState = &viewport_state_create_info;
        pipeline_create_info.pRasterizationState = &rasterizer_create_info;
        pipeline_create_info.pMultisampleState = &multisample_create_info;
        pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
        pipeline_create_info.pColorBlendState = &color_blend_create_info;
        pipeline_create_info.pDynamicState = &dynamic_state_create_info;
        pipeline_create_info.pTessellationState = nullptr;
        pipeline_create_info.layout = m_layout;
        pipeline_create_info.renderPass = create_info.render_pass->get();
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_create_info.basePipelineIndex = -1;

        VkResult result = vkCreateGraphicsPipelines(
            create_info.device->get(),
            VK_NULL_HANDLE,
            1,
            &pipeline_create_info,
            create_info.vulkan_allocator,
            &m_pipeline
        );
        if (!vk::is_success(result)) {
            ErrorLog("vkCreateGraphicsPipelines failed with {}.", vk::result_to_cstr(result, true));
            return;
        }

        InfoLog("Vulkan Graphics Pipeline created.");
    }

    void Pipeline::shutdown() {
        if (m_pipeline == nullptr)
            return;

        vkDestroyPipeline(m_device->get(), m_pipeline, m_vulkan_allocator);
        m_pipeline = nullptr;
        vkDestroyPipelineLayout(m_device->get(), m_layout, m_vulkan_allocator);
        m_layout = nullptr;

        InfoLog("Vulkan Graphics Pipeline destroyed.");
    }

    void Pipeline::bind(CommandBuffer* command_buffer, VkPipelineBindPoint bind_point) {
        vkCmdBindPipeline(command_buffer->get(), bind_point, m_pipeline);
    }
}
