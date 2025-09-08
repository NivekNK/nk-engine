#include "nkpch.h"

#include "vulkan/shaders/object_shader.h"

#include "vulkan/device.h"
#include "vulkan/shaders/utils.h"

#include <glm/ext/vector_float3.hpp>

#define BUILTIN_SHADER_NAME_OBJECT "Builtin.ObjectShader"

namespace nk {
    void ObjectShader::init(u32 width, u32 height, RenderPass* render_pass, Device* device, VkAllocationCallbacks* vulkan_allocator) {
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        char stage_type_strings[shader_stage_count][10] = { "vertex", "fragment" };
        VkShaderStageFlagBits stage_types[shader_stage_count] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
        for (u32 i = 0; i < shader_stage_count; i++) {
            DebugLog("Creating {} shader module for '{}'", stage_type_strings[i], BUILTIN_SHADER_NAME_OBJECT);
            if (!create_shader_module(BUILTIN_SHADER_NAME_OBJECT, stage_type_strings[i], device, vulkan_allocator, stage_types[i], &m_stages[i])) {
                ErrorLog("Unable to create {} shader module for '{}'", stage_type_strings[i], BUILTIN_SHADER_NAME_OBJECT);
                return;
            }
        }

        // TODO: Descriptors

        // Pipeline creation START
        // Viewport
        VkViewport viewport;
        viewport.x = 0.0f;
        viewport.y = static_cast<f32>(height);
        viewport.width = static_cast<f32>(width);
        viewport.height = -static_cast<f32>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // Scissor
        VkRect2D scissor;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = width;
        scissor.extent.height = height;

        // Attributes
        u32 offset = 0;
        constexpr u32 attribute_count = 1;
        VkVertexInputAttributeDescription attribute_descriptions[attribute_count];
        // Position
        VkFormat formats[attribute_count] = { 
            VK_FORMAT_R32G32B32_SFLOAT
        };
        u64 sizes[attribute_count] = {
            sizeof(glm::vec3)
        };
        for (u32 i = 0; i < attribute_count; i++) {
            attribute_descriptions[i].binding = 0;
            attribute_descriptions[i].location = i;
            attribute_descriptions[i].format = formats[i];
            attribute_descriptions[i].offset = offset;
            offset += sizes[i];
        }

        // TODO: Descriptor set layouts

        // Stages
        // NOTE: Should match the number of shader stages in the shader.
        VkPipelineShaderStageCreateInfo stages[shader_stage_count];
        for (u32 i = 0; i < shader_stage_count; i++) {
            stages[i] = m_stages[i].pipeline_create_info;
        }

        // Pipeline
        m_pipeline.init({
            .device = device,
            .vulkan_allocator = vulkan_allocator,
            .render_pass = render_pass,
            .attribute_count = attribute_count,
            .attributes = attribute_descriptions,
            .descriptor_set_layout_count = 0,
            .descriptor_set_layouts = nullptr,
            .stage_count = shader_stage_count,
            .stages = stages,
            .viewport = viewport,
            .scissor = scissor,
            .is_wireframe = false
        });
        // Pipeline creation END
    }

    void ObjectShader::shutdown() {
        m_pipeline.shutdown();

        for (u32 i = 0; i < shader_stage_count; i++) {
            if (m_stages[i].module != nullptr) {
                vkDestroyShaderModule(m_device->get(), m_stages[i].module, m_vulkan_allocator);
                m_stages[i].module = nullptr;
            }
        }

        m_device = nullptr;
        m_vulkan_allocator = nullptr;
    }

    void ObjectShader::use() {
        
    }
}
