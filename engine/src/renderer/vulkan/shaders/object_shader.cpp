#include "core/defines.h"
#include "nkpch.h"

#include "vulkan/shaders/object_shader.h"
#include "renderer/object_uniform_object.h"

#include "vulkan/device.h"
#include "vulkan/shaders/utils.h"
#include "vulkan/resources/texture_data.h"

#include "collections/dyarr.h"

#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>

#define BUILTIN_SHADER_NAME_OBJECT "Builtin.ObjectShader"

namespace nk {
    void ObjectShader::init(
        u32 width,
        u32 height,
        u32 image_count,
        RenderPass* render_pass,
        Device* device,
        VkAllocationCallbacks* vulkan_allocator) {
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;
        m_image_count = image_count;
        m_object_uniform_buffer_index = 0;

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
        global_descriptor_pool_size.descriptorCount = m_image_count;

        VkDescriptorPoolCreateInfo global_descriptor_pool_create_info;
        memset(&global_descriptor_pool_create_info, 0, sizeof(global_descriptor_pool_create_info));
        global_descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        global_descriptor_pool_create_info.poolSizeCount = 1;
        global_descriptor_pool_create_info.pPoolSizes = &global_descriptor_pool_size;
        global_descriptor_pool_create_info.maxSets = m_image_count;

        VulkanCheck(vkCreateDescriptorPool(m_device->get(), &global_descriptor_pool_create_info, m_vulkan_allocator, &m_global_descriptor_pool));

        // Local/Object descriptors
        constexpr u32 local_sampler_count = 1;
        VkDescriptorType descriptor_types[ObjectShaderObjectState::descriptor_count] = {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         // Binding 0: Uniform buffer
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER  // Binding 1: Diffuse sampler layout
        };
        VkDescriptorSetLayoutBinding bindings[ObjectShaderObjectState::descriptor_count];
        memset(bindings, 0, sizeof(VkDescriptorSetLayoutBinding) * ObjectShaderObjectState::descriptor_count);
        for (u32 i = 0; i < ObjectShaderObjectState::descriptor_count; i++) {
            bindings[i].binding = i;
            bindings[i].descriptorCount = 1;
            bindings[i].descriptorType = descriptor_types[i];
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layout_info;
        memset(&layout_info, 0, sizeof(layout_info));
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = ObjectShaderObjectState::descriptor_count;
        layout_info.pBindings = bindings;

        VulkanCheck(vkCreateDescriptorSetLayout(m_device->get(), &layout_info, m_vulkan_allocator, &m_object_descriptor_set_layout));

        // Local/Object descriptor pool: Used for object-specific items like diffuse color
        constexpr u32 object_pool_sizes_count = 2;
        VkDescriptorPoolSize object_pool_sizes[object_pool_sizes_count];
        // The first section will be used for uniform buffers
        object_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        object_pool_sizes[0].descriptorCount = object_max_object_count * m_image_count;
        // The second section will be used for image samplers
        object_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        object_pool_sizes[1].descriptorCount = local_sampler_count * object_max_object_count * m_image_count;

        VkDescriptorPoolCreateInfo object_pool_create_info;
        memset(&object_pool_create_info, 0, sizeof(object_pool_create_info));
        object_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        object_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        object_pool_create_info.poolSizeCount = object_pool_sizes_count;
        object_pool_create_info.pPoolSizes = object_pool_sizes;
        object_pool_create_info.maxSets = object_max_object_count * m_image_count;

        VulkanCheck(vkCreateDescriptorPool(m_device->get(), &object_pool_create_info, m_vulkan_allocator, &m_object_descriptor_pool));

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
        constexpr u32 attribute_count = 2;
        VkVertexInputAttributeDescription attribute_descriptions[attribute_count];
        // Position, texcoords
        VkFormat formats[attribute_count] = { 
            VK_FORMAT_R32G32B32_SFLOAT,
            VK_FORMAT_R32G32_SFLOAT,
        };
        u64 sizes[attribute_count] = {
            sizeof(glm::vec3),
            sizeof(glm::vec2),
        };
        for (u32 i = 0; i < attribute_count; i++) {
            attribute_descriptions[i].binding = 0;
            attribute_descriptions[i].location = i;
            attribute_descriptions[i].format = formats[i];
            attribute_descriptions[i].offset = offset;
            offset += sizes[i];
        }

        // Descriptor set layouts
        constexpr u32 descriptor_set_layout_count = 2;
        VkDescriptorSetLayout layouts[descriptor_set_layout_count] = {
            m_global_descriptor_set_layout,
            m_object_descriptor_set_layout,
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
            sizeof(GlobalUniformObject),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true);

        std::vector<VkDescriptorSetLayout> global_layouts(m_image_count, m_global_descriptor_set_layout);
        m_global_descriptor_sets.resize(m_image_count);

        VkDescriptorSetAllocateInfo global_descriptor_set_allocate_info;
        memset(&global_descriptor_set_allocate_info, 0, sizeof(global_descriptor_set_allocate_info));
        global_descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        global_descriptor_set_allocate_info.descriptorPool = m_global_descriptor_pool;
        global_descriptor_set_allocate_info.descriptorSetCount = m_image_count;
        global_descriptor_set_allocate_info.pSetLayouts = global_layouts.data();
        VulkanCheck(vkAllocateDescriptorSets(m_device->get(), &global_descriptor_set_allocate_info, m_global_descriptor_sets.data()));

        // Initialize the object uniform buffer
        m_object_uniform_buffer.init(
            m_device,
            m_vulkan_allocator,
            sizeof(ObjectUniformObject) * object_max_object_count,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true);
    }

    void ObjectShader::shutdown() {
        // Guard against double shutdown
        if (m_device == nullptr) {
            DebugLog("ObjectShader::shutdown() - Already shutdown, skipping");
            return;
        }

        // Destroy object uniform buffer
        m_object_uniform_buffer.shutdown();

        // Destroy global uniform buffer
        m_global_uniform_buffer.shutdown();

        m_pipeline.shutdown();

        // Destroy object descriptor pool
        vkDestroyDescriptorPool(m_device->get(), m_object_descriptor_pool, m_vulkan_allocator);

        // Destroy object descriptor set layout
        vkDestroyDescriptorSetLayout(m_device->get(), m_object_descriptor_set_layout, m_vulkan_allocator);

        // Destroy global descriptor pool
        vkDestroyDescriptorPool(m_device->get(), m_global_descriptor_pool, m_vulkan_allocator);

        // Destroy global descriptor set layout
        vkDestroyDescriptorSetLayout(m_device->get(), m_global_descriptor_set_layout, m_vulkan_allocator);

        m_global_descriptor_sets.clear();
        for (u32 i = 0; i < m_object_uniform_buffer_index; ++i) {
            m_object_states[i].descriptor_sets.clear();
            for (u32 j = 0; j < ObjectShaderObjectState::descriptor_count; ++j)
                m_object_states[i].descriptor_states[j].generations.clear();
        }

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

    void ObjectShader::update_global_state(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index, f32 delta_time) {
        if (image_index >= m_global_descriptor_sets.size()) {
            ErrorLog("ObjectShader global descriptor index '{}' is out of range ({}).", image_index, m_global_descriptor_sets.size());
            return;
        }

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

    void ObjectShader::update_object(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index, GeometryRenderData data, f32 delta_time) {
        if (data.object_id >= m_object_uniform_buffer_index ||
            image_index >= m_object_states[data.object_id].descriptor_sets.size()) {
            ErrorLog("ObjectShader object or image index is out of range.");
            return;
        }

        VkCommandBuffer command_buffer = command_buffers[image_index].get();
        vkCmdPushConstants(command_buffer, m_pipeline.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &data.model);

        // Obtain material data
        ObjectShaderObjectState* object_state = &m_object_states[data.object_id];
        VkDescriptorSet object_descriptor_set = object_state->descriptor_sets[image_index];

        // TODO: if needs update
        VkWriteDescriptorSet descriptor_writes[ObjectShaderObjectState::descriptor_count];
        memset(descriptor_writes, 0, sizeof(VkWriteDescriptorSet) * ObjectShaderObjectState::descriptor_count);
        u32 descriptor_count = 0;
        u32 descriptor_index = 0;

        // Descriptor 0: Uniform Buffer
        u32 range = sizeof(ObjectUniformObject);
        u64 offset = sizeof(ObjectUniformObject) * data.object_id;
        ObjectUniformObject obo;

        // TODO: get diffuse color from a material
        static f32 accumulator = 0.0f;
        accumulator += delta_time;
        f32 s = (glm::sin(accumulator) + 1.0f) * 0.5f;
        obo.diffuse_color = glm::vec4(s, s, s, 1.0f);

        // Load the data into the buffer
        m_object_uniform_buffer.load_data(offset, range, 0, &obo);

        // Only do thisd if the descriptor has not yet been updated
        VkDescriptorBufferInfo buffer_info{};
        if (object_state->descriptor_states[descriptor_index].generations[image_index] == numeric::invalid_id) {
            buffer_info.buffer = m_object_uniform_buffer;
            buffer_info.offset = offset;
            buffer_info.range = range;

            VkWriteDescriptorSet descriptor;
            memset(&descriptor, 0, sizeof(VkWriteDescriptorSet));
            descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor.dstSet = object_descriptor_set;
            descriptor.dstBinding = descriptor_index;
            descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor.descriptorCount = 1;
            descriptor.pBufferInfo = &buffer_info;

            descriptor_writes[descriptor_count] = descriptor;
            descriptor_count++;

            // Update the frame generation. In this case it is only needed once since this is a buffer
            object_state->descriptor_states[descriptor_index].generations[image_index] = 1;
        }
        descriptor_index++;

        // TODO: samplers
        constexpr u32 sampler_count = 1;
        VkDescriptorImageInfo image_infos[1];
        for (u32 sampler_index = 0; sampler_index < sampler_count; sampler_index++) {
            Texture* texture = data.textures[sampler_index];
            u32* descriptor_generation = &object_state->descriptor_states[descriptor_index].generations[image_index];

            // Check if the descriptor needs updating first
            if (texture && (*descriptor_generation != texture->generation || *descriptor_generation == numeric::invalid_id)) {
                TextureData* internal_data = static_cast<TextureData*>(texture->m_internal_data);

                // Assign view and sampler
                image_infos[sampler_index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                image_infos[sampler_index].imageView = internal_data->image.get_view();
                image_infos[sampler_index].sampler = internal_data->sampler;

                VkWriteDescriptorSet descriptor;
                memset(&descriptor, 0, sizeof(VkWriteDescriptorSet));
                descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor.dstSet = object_descriptor_set;
                descriptor.dstBinding = descriptor_index;
                descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptor.descriptorCount = 1;
                descriptor.pImageInfo = &image_infos[sampler_index];

                descriptor_writes[descriptor_count] = descriptor;
                descriptor_count++;

                // Sync frame generation if not using a default texture
                if (texture->generation != numeric::invalid_id) {
                    *descriptor_generation = texture->generation;
                }
                descriptor_index++;
            }
        }

        if (descriptor_count > 0) {
            vkUpdateDescriptorSets(m_device->get(), descriptor_count, descriptor_writes, 0, nullptr);
        }

        // Bind the descriptor set to be updated, or in case the shader changed
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline.get_layout(),
            1,
            1,
            &object_descriptor_set,
            0,
            nullptr
        );
    }

    bool ObjectShader::acquire_resources(u32* out_object_id) {
        // TODO: free list
        if (m_object_uniform_buffer_index >= object_max_object_count) {
            ErrorLog("ObjectShader has no free object descriptor slots.");
            return false;
        }

        *out_object_id = m_object_uniform_buffer_index;
        m_object_uniform_buffer_index++;
        
        u32 object_id = *out_object_id;
        ObjectShaderObjectState* object_state = &m_object_states[object_id];
        object_state->descriptor_sets.resize(m_image_count);
        for (u32 i = 0; i < ObjectShaderObjectState::descriptor_count; i++) {
            object_state->descriptor_states[i].generations.assign(m_image_count, numeric::invalid_id);
        }

        // Allocate descriptor sets
        std::vector<VkDescriptorSetLayout> layouts(m_image_count, m_object_descriptor_set_layout);

        VkDescriptorSetAllocateInfo alloc_info;
        memset(&alloc_info, 0, sizeof(VkDescriptorSetAllocateInfo));
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = m_object_descriptor_pool;
        alloc_info.descriptorSetCount = m_image_count;
        alloc_info.pSetLayouts = layouts.data();

        VkResult results = vkAllocateDescriptorSets(m_device->get(), &alloc_info, object_state->descriptor_sets.data());
        if (results != VK_SUCCESS) {
            ErrorLog("Error allocating descriptor sets in shader!");
            return false;
        }

        return true;
    }

    void ObjectShader::release_resources(u32 object_id) {
        ObjectShaderObjectState* object_state = &m_object_states[object_id];

        const u32 descriptor_set_count = object_state->descriptor_sets.size();
        // Release object descriptor sets
        VkResult result = vkFreeDescriptorSets(
            m_device->get(),
            m_object_descriptor_pool,
            descriptor_set_count,
            object_state->descriptor_sets.data());
        if (result != VK_SUCCESS) {
            ErrorLog("Error freeing object shader descriptor sets!");
        }

        for (u32 i = 0; i < ObjectShaderObjectState::descriptor_count; i++) {
            for (u32 j = 0; j < object_state->descriptor_states[i].generations.size(); j++) {
                object_state->descriptor_states[i].generations[j] = numeric::invalid_id;
            }
        }
        object_state->descriptor_sets.clear();

        // TODO: add the object_id to the free list
    }
}
