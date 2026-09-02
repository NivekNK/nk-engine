#include "core/defines.h"
#include "nkpch.h"

#include "vulkan/shaders/material_shader.h"
#include "renderer/material_uniform_object.h"

#include "vulkan/device.h"
#include "vulkan/shaders/utils.h"
#include "vulkan/resources/texture_data.h"

#include "collections/dyarr.h"

#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>

#define BUILTIN_SHADER_NAME_MATERIAL "Builtin.MaterialShader"

namespace nk {
    namespace {
        renderer_error translate_shader_error(const shader_error& error) {
            switch (error.code) {
                case shader_error_code::path_format_failed:
                    return {renderer_error_code::shader_path_failed, 0};
                case shader_error_code::file_failed:
                    return {
                        renderer_error_code::shader_file_failed,
                        static_cast<i32>(error.file),
                    };
                case shader_error_code::invalid_binary:
                    return {renderer_error_code::shader_binary_invalid, 0};
                case shader_error_code::module_creation_failed:
                    return {
                        renderer_error_code::shader_module_creation_failed,
                        static_cast<i32>(error.native_code),
                    };
                case shader_error_code::out_of_memory:
                    return {renderer_error_code::out_of_memory, 0};
            }
            return {renderer_error_code::initialization_failed, 0};
        }

        renderer_error descriptor_error(const VkResult result) {
            return {
                renderer_error_code::descriptor_creation_failed,
                static_cast<i32>(result),
            };
        }
    }

    result<void, renderer_error> MaterialShader::init(
        u32 width,
        u32 height,
        u32 image_count,
        RenderPass* render_pass,
        Device* device,
        mem::Allocator* allocator,
        VkAllocationCallbacks* vulkan_allocator,
        Texture* default_texture) {
        m_device = device;
        m_allocator = allocator;
        m_vulkan_allocator = vulkan_allocator;
        m_default_texture = default_texture;
        m_image_count = image_count;

        char stage_type_strings[shader_stage_count][10] = { "vertex", "fragment" };
        VkShaderStageFlagBits stage_types[shader_stage_count] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
        for (u32 i = 0; i < shader_stage_count; i++) {
            DebugLog("Creating {} shader module for '{}'", stage_type_strings[i], BUILTIN_SHADER_NAME_MATERIAL);
            auto created = create_shader_module(
                BUILTIN_SHADER_NAME_MATERIAL,
                stage_type_strings[i],
                m_device,
                m_vulkan_allocator,
                stage_types[i],
                &m_stages[i]);
            if (!created) {
                return err(translate_shader_error(created.error()));
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

        VkResult result = vkCreateDescriptorSetLayout(
            m_device->get(),
            &global_layout_create_info,
            m_vulkan_allocator,
            &m_global_descriptor_set_layout);
        if (result != VK_SUCCESS)
            return err(descriptor_error(result));

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

        result = vkCreateDescriptorPool(
            m_device->get(),
            &global_descriptor_pool_create_info,
            m_vulkan_allocator,
            &m_global_descriptor_pool);
        if (result != VK_SUCCESS)
            return err(descriptor_error(result));

        // Local/Object descriptors
        constexpr u32 local_sampler_count = 1;
        VkDescriptorType descriptor_types[MaterialShaderInstanceState::descriptor_count] = {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         // Binding 0: Uniform buffer
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER  // Binding 1: Diffuse sampler layout
        };
        VkDescriptorSetLayoutBinding bindings[MaterialShaderInstanceState::descriptor_count];
        memset(bindings, 0, sizeof(VkDescriptorSetLayoutBinding) * MaterialShaderInstanceState::descriptor_count);
        for (u32 i = 0; i < MaterialShaderInstanceState::descriptor_count; i++) {
            bindings[i].binding = i;
            bindings[i].descriptorCount = 1;
            bindings[i].descriptorType = descriptor_types[i];
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layout_info;
        memset(&layout_info, 0, sizeof(layout_info));
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = MaterialShaderInstanceState::descriptor_count;
        layout_info.pBindings = bindings;

        result = vkCreateDescriptorSetLayout(
            m_device->get(),
            &layout_info,
            m_vulkan_allocator,
            &m_object_descriptor_set_layout);
        if (result != VK_SUCCESS)
            return err(descriptor_error(result));

        // Local/Object descriptor pool: Used for object-specific items like diffuse color
        constexpr u32 object_pool_sizes_count = 2;
        VkDescriptorPoolSize object_pool_sizes[object_pool_sizes_count];
        // The first section will be used for uniform buffers
        object_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        object_pool_sizes[0].descriptorCount = max_material_count * m_image_count;
        // The second section will be used for image samplers
        object_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        object_pool_sizes[1].descriptorCount = local_sampler_count * max_material_count * m_image_count;

        VkDescriptorPoolCreateInfo object_pool_create_info;
        memset(&object_pool_create_info, 0, sizeof(object_pool_create_info));
        object_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        object_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        object_pool_create_info.poolSizeCount = object_pool_sizes_count;
        object_pool_create_info.pPoolSizes = object_pool_sizes;
        object_pool_create_info.maxSets = max_material_count * m_image_count;

        result = vkCreateDescriptorPool(
            m_device->get(),
            &object_pool_create_info,
            m_vulkan_allocator,
            &m_object_descriptor_pool);
        if (result != VK_SUCCESS)
            return err(descriptor_error(result));

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
        auto pipeline_initialized = m_pipeline.init({
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
        if (!pipeline_initialized)
            return err(pipeline_initialized.error());
        // Pipeline creation END

        // Initialize the global uniform buffer
        const VkMemoryPropertyFlags optional_device_local =
            m_device->supports_device_local_host_visible()
                ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                : 0;
        auto global_buffer_initialized = m_global_uniform_buffer.init(
            m_device,
            m_vulkan_allocator,
            sizeof(GlobalUniformObject),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                optional_device_local,
            true);
        if (!global_buffer_initialized)
            return err(global_buffer_initialized.error());

        cl::arr<VkDescriptorSetLayout> global_layouts;
        if (!global_layouts.arr_init(m_allocator, m_image_count) ||
            !m_global_descriptor_sets.arr_init(m_allocator, m_image_count)) {
            global_layouts.arr_shutdown();
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        }
        for (VkDescriptorSetLayout& layout : global_layouts)
            layout = m_global_descriptor_set_layout;

        VkDescriptorSetAllocateInfo global_descriptor_set_allocate_info;
        memset(&global_descriptor_set_allocate_info, 0, sizeof(global_descriptor_set_allocate_info));
        global_descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        global_descriptor_set_allocate_info.descriptorPool = m_global_descriptor_pool;
        global_descriptor_set_allocate_info.descriptorSetCount = m_image_count;
        global_descriptor_set_allocate_info.pSetLayouts = global_layouts.data();
        result = vkAllocateDescriptorSets(
            m_device->get(),
            &global_descriptor_set_allocate_info,
            m_global_descriptor_sets.data());
        global_layouts.arr_shutdown();
        if (result != VK_SUCCESS)
            return err(descriptor_error(result));

        // Initialize the material uniform buffer
        auto material_buffer_initialized = m_material_uniform_buffer.init(
            m_device,
            m_vulkan_allocator,
            sizeof(MaterialUniformObject) * max_material_count,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true);
        if (!material_buffer_initialized)
            return err(material_buffer_initialized.error());

        return ok();
    }

    void MaterialShader::shutdown() {
        // Guard against double shutdown
        if (m_device == nullptr)
            return;

        // Destroy material uniform buffer
        m_material_uniform_buffer.shutdown();

        // Destroy global uniform buffer
        m_global_uniform_buffer.shutdown();

        m_pipeline.shutdown();

        if (m_object_descriptor_pool != nullptr) {
            vkDestroyDescriptorPool(
                m_device->get(), m_object_descriptor_pool, m_vulkan_allocator);
            m_object_descriptor_pool = nullptr;
        }

        if (m_object_descriptor_set_layout != nullptr) {
            vkDestroyDescriptorSetLayout(
                m_device->get(), m_object_descriptor_set_layout, m_vulkan_allocator);
            m_object_descriptor_set_layout = nullptr;
        }

        if (m_global_descriptor_pool != nullptr) {
            vkDestroyDescriptorPool(
                m_device->get(), m_global_descriptor_pool, m_vulkan_allocator);
            m_global_descriptor_pool = nullptr;
        }

        if (m_global_descriptor_set_layout != nullptr) {
            vkDestroyDescriptorSetLayout(
                m_device->get(), m_global_descriptor_set_layout, m_vulkan_allocator);
            m_global_descriptor_set_layout = nullptr;
        }

        m_global_descriptor_sets.arr_shutdown();
        for (MaterialShaderInstanceState& instance : m_instance_states) {
            if (instance.descriptor_sets.allocator() == nullptr)
                continue;
            (void)instance.descriptor_sets.arr_shutdown();
            for (DescriptorState& state : instance.descriptor_states) {
                (void)state.generations.arr_shutdown();
                (void)state.ids.arr_shutdown();
            }
        }

        for (u32 i = 0; i < shader_stage_count; i++) {
            if (m_stages[i].module != nullptr) {
                vkDestroyShaderModule(m_device->get(), m_stages[i].module, m_vulkan_allocator);
                m_stages[i].module = nullptr;
            }
        }

        m_device = nullptr;
        m_allocator = nullptr;
        m_vulkan_allocator = nullptr;
        m_default_texture = nullptr;
    }

    void MaterialShader::use(CommandBuffer* command_buffer) {
        m_pipeline.bind(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);
    }

    void MaterialShader::update_global_state(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index, f32 delta_time) {
        if (image_index >= m_global_descriptor_sets.length()) {
            ErrorLog("MaterialShader global descriptor index '{}' is out of range ({}).", image_index, m_global_descriptor_sets.length());
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

    void MaterialShader::update_object(
        const cl::dyarr<CommandBuffer>& command_buffers,
        const u32 image_index,
        const GeometryRenderData data) {
        Material* material = data.material;
        if (material == nullptr || !material->valid() ||
            material->internal_id >= max_material_count ||
            image_index >= command_buffers.length()) {
            ErrorLog("MaterialShader received an invalid material or image index.");
            return;
        }

        VkCommandBuffer command_buffer = command_buffers[image_index].get();
        vkCmdPushConstants(command_buffer, m_pipeline.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &data.model);

        MaterialShaderInstanceState& instance =
            m_instance_states[material->internal_id];
        if (image_index >= instance.descriptor_sets.length()) {
            ErrorLog("MaterialShader descriptor image index is out of range.");
            return;
        }
        VkDescriptorSet descriptor_set = instance.descriptor_sets[image_index];

        VkWriteDescriptorSet descriptor_writes[MaterialShaderInstanceState::descriptor_count];
        memset(descriptor_writes, 0, sizeof(VkWriteDescriptorSet) * MaterialShaderInstanceState::descriptor_count);
        u32 descriptor_count = 0;
        u32 descriptor_index = 0;

        const u32 range = sizeof(MaterialUniformObject);
        const u64 offset =
            sizeof(MaterialUniformObject) * material->internal_id;
        MaterialUniformObject ubo{};
        ubo.diffuse_color = material->diffuse_color;

        m_material_uniform_buffer.load_data(offset, range, 0, &ubo);

        VkDescriptorBufferInfo buffer_info{};
        u32& uniform_generation =
            instance.descriptor_states[descriptor_index].generations[image_index];
        if (uniform_generation != material->generation) {
            buffer_info.buffer = m_material_uniform_buffer;
            buffer_info.offset = offset;
            buffer_info.range = range;

            VkWriteDescriptorSet descriptor{};
            descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor.dstSet = descriptor_set;
            descriptor.dstBinding = descriptor_index;
            descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor.descriptorCount = 1;
            descriptor.pBufferInfo = &buffer_info;
            descriptor_writes[descriptor_count++] = descriptor;
            uniform_generation = material->generation;
        }
        ++descriptor_index;

        Texture* texture = material->diffuse_map.texture;
        if (texture == nullptr || !texture->valid())
            texture = m_default_texture;
        if (texture == nullptr || !texture->valid()) {
            ErrorLog("MaterialShader has no valid diffuse texture.");
            return;
        }

        DescriptorState& sampler_state =
            instance.descriptor_states[descriptor_index];
        u32& sampler_generation = sampler_state.generations[image_index];
        u32& sampler_id = sampler_state.ids[image_index];
        VkDescriptorImageInfo image_info{};
        if (sampler_generation != texture->generation ||
            sampler_id != texture->id) {
            TextureData* internal_data =
                static_cast<TextureData*>(texture->m_internal_data);
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = internal_data->image.get_view();
            image_info.sampler = internal_data->sampler;

            VkWriteDescriptorSet descriptor{};
            descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor.dstSet = descriptor_set;
            descriptor.dstBinding = descriptor_index;
            descriptor.descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor.descriptorCount = 1;
            descriptor.pImageInfo = &image_info;
            descriptor_writes[descriptor_count++] = descriptor;
            sampler_generation = texture->generation;
            sampler_id = texture->id;
        }

        if (descriptor_count > 0)
            vkUpdateDescriptorSets(m_device->get(), descriptor_count, descriptor_writes, 0, nullptr);

        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline.get_layout(),
            1,
            1,
            &descriptor_set,
            0,
            nullptr
        );
    }

    result<void, renderer_error> MaterialShader::acquire_resources(
        Material& material) {
        u32 instance_id = numeric::invalid_id;
        for (u32 index = 0; index < max_material_count; ++index) {
            if (m_instance_states[index].descriptor_sets.allocator() == nullptr) {
                instance_id = index;
                break;
            }
        }
        if (instance_id == numeric::invalid_id) {
            return err(renderer_error{
                renderer_error_code::object_resource_failed,
                0,
            });
        }

        MaterialShaderInstanceState& instance = m_instance_states[instance_id];
        if (!instance.descriptor_sets.arr_init(m_allocator, m_image_count))
            return err(renderer_error{renderer_error_code::out_of_memory, 0});

        for (u32 i = 0; i < MaterialShaderInstanceState::descriptor_count; i++) {
            DescriptorState& state = instance.descriptor_states[i];
            if (!state.generations.arr_init(m_allocator, m_image_count) ||
                !state.ids.arr_init(m_allocator, m_image_count)) {
                for (DescriptorState& initialized : instance.descriptor_states) {
                    if (initialized.generations.allocator() != nullptr)
                        (void)initialized.generations.arr_shutdown();
                    if (initialized.ids.allocator() != nullptr)
                        (void)initialized.ids.arr_shutdown();
                }
                (void)instance.descriptor_sets.arr_shutdown();
                return err(renderer_error{
                    renderer_error_code::out_of_memory,
                    0,
                });
            }
            for (u32 image = 0; image < m_image_count; ++image) {
                state.generations[image] = numeric::invalid_id;
                state.ids[image] = numeric::invalid_id;
            }
        }

        cl::arr<VkDescriptorSetLayout> layouts;
        if (!layouts.arr_init(m_allocator, m_image_count)) {
            for (DescriptorState& state : instance.descriptor_states) {
                (void)state.generations.arr_shutdown();
                (void)state.ids.arr_shutdown();
            }
            (void)instance.descriptor_sets.arr_shutdown();
            return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }
        for (VkDescriptorSetLayout& layout : layouts)
            layout = m_object_descriptor_set_layout;

        VkDescriptorSetAllocateInfo alloc_info;
        memset(&alloc_info, 0, sizeof(VkDescriptorSetAllocateInfo));
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = m_object_descriptor_pool;
        alloc_info.descriptorSetCount = m_image_count;
        alloc_info.pSetLayouts = layouts.data();

        const VkResult allocation_result = vkAllocateDescriptorSets(
            m_device->get(),
            &alloc_info,
            instance.descriptor_sets.data());
        (void)layouts.arr_shutdown();
        if (allocation_result != VK_SUCCESS) {
            for (DescriptorState& state : instance.descriptor_states) {
                (void)state.generations.arr_shutdown();
                (void)state.ids.arr_shutdown();
            }
            (void)instance.descriptor_sets.arr_shutdown();
            return err(renderer_error{
                renderer_error_code::descriptor_creation_failed,
                static_cast<i32>(allocation_result),
            });
        }

        material.internal_id = instance_id;
        return ok();
    }

    void MaterialShader::release_resources(Material& material) {
        if (material.internal_id >= max_material_count)
            return;
        MaterialShaderInstanceState& instance =
            m_instance_states[material.internal_id];
        if (instance.descriptor_sets.allocator() == nullptr) {
            material.internal_id = numeric::invalid_id;
            return;
        }

        vkDeviceWaitIdle(m_device->get());
        const u32 descriptor_set_count =
            static_cast<u32>(instance.descriptor_sets.length());
        const VkResult release_result = vkFreeDescriptorSets(
            m_device->get(),
            m_object_descriptor_pool,
            descriptor_set_count,
            instance.descriptor_sets.data());
        if (release_result != VK_SUCCESS)
            ErrorLog("Failed to free material descriptor sets.");

        for (DescriptorState& state : instance.descriptor_states) {
            (void)state.generations.arr_shutdown();
            (void)state.ids.arr_shutdown();
        }
        (void)instance.descriptor_sets.arr_shutdown();
        material.internal_id = numeric::invalid_id;
    }
}
