#include "nkpch.h"

#include "vulkan/shaders/object_shader.h"

#include "vulkan/device.h"
#include "vulkan/shaders/utils.h"

#include "collections/dyarr.h"

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
            if (!create_shader_module(BUILTIN_SHADER_NAME_OBJECT, stage_type_strings[i], m_device, m_vulkan_allocator, stage_types[i], &m_stages[i])) {
                ErrorLog("Unable to create {} shader module for '{}'", stage_type_strings[i], BUILTIN_SHADER_NAME_OBJECT);
                return;
            }
        }

        // Global Descriptors
        VkDescriptorSetLayoutBinding global_ubo_set_layout_binding;
        memset(&global_ubo_set_layout_binding, 0, sizeof(global_ubo_set_layout_binding));
        global_ubo_set_layout_binding.binding = 0;
        global_ubo_set_layout_binding.descriptorCount = 1;
        global_ubo_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        global_ubo_set_layout_binding.pImmutableSamplers = nullptr;
        global_ubo_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo global_layout_create_info;
        memset(&global_layout_create_info, 0, sizeof(global_layout_create_info));
        global_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        global_layout_create_info.bindingCount = 1;
        global_layout_create_info.pBindings = &global_ubo_set_layout_binding;

        VulkanCheck(vkCreateDescriptorSetLayout(m_device->get(), &global_layout_create_info, m_vulkan_allocator, &m_global_descriptor_set_layout));

        // Global descriptor pool: Used for global items such as view/projection matrices
        VkDescriptorPoolSize global_descriptor_pool_size;
        global_descriptor_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        global_descriptor_pool_size.descriptorCount = global_descriptor_set_count; // Can also be the swapchain image_count

        VkDescriptorPoolCreateInfo global_descriptor_pool_create_info;
        memset(&global_descriptor_pool_create_info, 0, sizeof(global_descriptor_pool_create_info));
        global_descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        global_descriptor_pool_create_info.poolSizeCount = 1;
        global_descriptor_pool_create_info.pPoolSizes = &global_descriptor_pool_size;
        global_descriptor_pool_create_info.maxSets = global_descriptor_set_count;

        VulkanCheck(vkCreateDescriptorPool(m_device->get(), &global_descriptor_pool_create_info, m_vulkan_allocator, &m_global_descriptor_pool));

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

        // Descriptor set layouts
        constexpr u32 descriptor_set_layout_count = 1;
        VkDescriptorSetLayout layouts[descriptor_set_layout_count] = {
            m_global_descriptor_set_layout
        };

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
            .descriptor_set_layout_count = descriptor_set_layout_count,
            .descriptor_set_layouts = layouts,
            .stage_count = shader_stage_count,
            .stages = stages,
            .viewport = viewport,
            .scissor = scissor,
            .is_wireframe = false
        });
        // Pipeline creation END

        // Initialize the global uniform buffer
        m_global_uniform_buffer.init(
            m_device,
            m_vulkan_allocator,
            sizeof(GlobalUniformObject) * 3,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true);

        VkDescriptorSetLayout global_layouts[global_descriptor_set_count] = {
            m_global_descriptor_set_layout,
            m_global_descriptor_set_layout,
            m_global_descriptor_set_layout,
        };

        VkDescriptorSetAllocateInfo global_descriptor_set_allocate_info;
        memset(&global_descriptor_set_allocate_info, 0, sizeof(global_descriptor_set_allocate_info));
        global_descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        global_descriptor_set_allocate_info.descriptorPool = m_global_descriptor_pool;
        global_descriptor_set_allocate_info.descriptorSetCount = global_descriptor_set_count;
        global_descriptor_set_allocate_info.pSetLayouts = global_layouts;
        VulkanCheck(vkAllocateDescriptorSets(m_device->get(), &global_descriptor_set_allocate_info, m_global_descriptor_sets));
    }

    void ObjectShader::shutdown() {
        // Destroy global uniform buffer
        m_global_uniform_buffer.shutdown();

        m_pipeline.shutdown();

        // Destroy global descriptor pool
        vkDestroyDescriptorPool(m_device->get(), m_global_descriptor_pool, m_vulkan_allocator);

        // Destroy descriptor set layouts
        vkDestroyDescriptorSetLayout(m_device->get(), m_global_descriptor_set_layout, m_vulkan_allocator);

        for (u32 i = 0; i < shader_stage_count; i++) {
            if (m_stages[i].module != nullptr) {
                vkDestroyShaderModule(m_device->get(), m_stages[i].module, m_vulkan_allocator);
                m_stages[i].module = nullptr;
            }
        }

        m_device = nullptr;
        m_vulkan_allocator = nullptr;
    }

    void ObjectShader::use(CommandBuffer* command_buffer) {
        m_pipeline.bind(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    }

    void ObjectShader::update_global_state(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index) {
        VkCommandBuffer command_buffer = command_buffers[image_index].get();
        VkDescriptorSet global_descriptor = m_global_descriptor_sets[image_index];

        // Bind the global descriptor set to be updated
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline.get_layout(),
            0,
            1,
            &global_descriptor,
            0,
            nullptr
        );

        u32 range = sizeof(GlobalUniformObject);
        u64 offset = 0;

        // Copy data to buffer
        m_global_uniform_buffer.load_data(offset, range, 0, &m_global_ubo);

        VkDescriptorBufferInfo global_descriptor_buffer_info;
        global_descriptor_buffer_info.buffer = m_global_uniform_buffer;
        global_descriptor_buffer_info.offset = offset;
        global_descriptor_buffer_info.range = range;

        // Update descriptor set
        VkWriteDescriptorSet global_descriptor_write;
        memset(&global_descriptor_write, 0, sizeof(global_descriptor_write));
        global_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        global_descriptor_write.dstSet = global_descriptor;
        global_descriptor_write.dstBinding = 0;
        global_descriptor_write.dstArrayElement = 0;
        global_descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        global_descriptor_write.descriptorCount = 1;
        global_descriptor_write.pBufferInfo = &global_descriptor_buffer_info;

        vkUpdateDescriptorSets(m_device->get(), 1, &global_descriptor_write, 0, nullptr);
    }
}
