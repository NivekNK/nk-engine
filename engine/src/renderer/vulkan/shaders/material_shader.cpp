#include "nkpch.h"

#include "vulkan/shaders/material_shader.h"

#include "renderer/material_uniform_object.h"
#include "vulkan/resources/texture_data.h"

namespace nk {
    namespace {
        constexpr u32 uniform_descriptor_index = 0;
        constexpr u32 sampler_descriptor_index = 1;

        renderer_error invalid_material_shader_config() noexcept {
            return {
                renderer_error_code::shader_config_invalid,
                static_cast<i32>(shader_config_error::unsupported_layout),
            };
        }

        void release_instance_metadata(
            MaterialShaderInstanceState& instance) noexcept {
            for (DescriptorState& state : instance.descriptor_states) {
                (void)state.generations.arr_shutdown();
                (void)state.ids.arr_shutdown();
            }
            (void)instance.descriptor_sets.arr_shutdown();
        }
    }

    result<void, renderer_error> MaterialShader::init(
        const ShaderConfig& config,
        const u32 width,
        const u32 height,
        const u32 image_count,
        RenderPass* render_pass,
        Device* device,
        mem::Allocator* allocator,
        ResourceSystem* resources,
        VkAllocationCallbacks* vulkan_allocator,
        Texture* default_texture) {
        if (m_shader.initialized() || allocator == nullptr ||
            config.max_instances != max_material_count ||
            config.descriptor_sets.length() != 2 ||
            config.descriptor_sets[0].scope != ShaderScope::global ||
            config.descriptor_sets[1].scope != ShaderScope::instance ||
            config.descriptor_sets[1].bindings.length() !=
                MaterialShaderInstanceState::descriptor_count) {
            return err(invalid_material_shader_config());
        }

        m_allocator = allocator;
        m_default_texture = default_texture;
        m_image_count = image_count;
        auto initialized = m_shader.init(
            config,
            width,
            height,
            image_count,
            render_pass,
            device,
            allocator,
            resources,
            vulkan_allocator);
        if (!initialized) {
            m_allocator = nullptr;
            m_default_texture = nullptr;
            m_image_count = 0;
            return err(initialized.error());
        }

        if (m_shader.instance_uniform_binding() == numeric::invalid_id ||
            m_shader.instance_sampler_binding() == numeric::invalid_id ||
            m_shader.instance_uniform_size() != sizeof(MaterialUniformObject)) {
            shutdown();
            return err(invalid_material_shader_config());
        }
        return ok();
    }

    void MaterialShader::shutdown() {
        for (MaterialShaderInstanceState& instance : m_instance_states) {
            if (instance.descriptor_sets.allocator() != nullptr)
                release_instance_metadata(instance);
        }
        m_shader.shutdown();
        m_allocator = nullptr;
        m_default_texture = nullptr;
        m_image_count = 0;
        m_global_ubo = {};
    }

    void MaterialShader::use(CommandBuffer* command_buffer) {
        if (command_buffer != nullptr && m_shader.initialized())
            m_shader.use(*command_buffer);
    }

    void MaterialShader::update_global_state(
        CommandBuffer& command_buffer,
        const u32 image_index) {
        auto applied = m_shader.apply_global_uniform(
            command_buffer,
            image_index,
            &m_global_ubo,
            sizeof(m_global_ubo));
        if (!applied) {
            ErrorLog(
                "MaterialShader failed to apply globals: renderer_error={}, native_code={}.",
                static_cast<u32>(applied.error().code),
                applied.error().native_code);
        }
    }

    void MaterialShader::set_model(
        CommandBuffer& command_buffer,
        const glm::mat4& model) {
        auto pushed = m_shader.push_constant(
            command_buffer,
            ShaderStage::vertex,
            0,
            sizeof(model),
            &model);
        if (!pushed) {
            ErrorLog(
                "MaterialShader failed to set the model push constant: renderer_error={}, native_code={}.",
                static_cast<u32>(pushed.error().code),
                pushed.error().native_code);
        }
    }

    void MaterialShader::apply_material(
        const cl::dyarr<CommandBuffer>& command_buffers,
        const u32 image_index,
        Material& material) {
        if (!material.valid() ||
            material.internal_id >= max_material_count ||
            image_index >= command_buffers.length() ||
            image_index >= m_image_count) {
            ErrorLog(
                "MaterialShader received an invalid material or image index.");
            return;
        }

        const CommandBuffer& command_buffer = command_buffers[image_index];
        MaterialShaderInstanceState& instance =
            m_instance_states[material.internal_id];
        if (image_index >= instance.descriptor_sets.length()) {
            ErrorLog("MaterialShader descriptor image index is out of range.");
            return;
        }
        const VkDescriptorSet descriptor_set =
            instance.descriptor_sets[image_index];

        MaterialUniformObject ubo{};
        ubo.diffuse_color = material.diffuse_color;
        auto loaded = m_shader.load_instance_uniform(
            material.internal_id, &ubo, sizeof(ubo));
        if (!loaded) {
            ErrorLog(
                "MaterialShader failed to update an instance uniform: renderer_error={}, native_code={}.",
                static_cast<u32>(loaded.error().code),
                loaded.error().native_code);
            return;
        }

        VkWriteDescriptorSet descriptor_writes[
            MaterialShaderInstanceState::descriptor_count]{};
        u32 descriptor_count = 0;

        VkDescriptorBufferInfo buffer_info{};
        u32& uniform_generation =
            instance.descriptor_states[uniform_descriptor_index]
                .generations[image_index];
        if (uniform_generation != material.generation) {
            buffer_info.buffer = m_shader.instance_uniform_buffer();
            buffer_info.offset =
                m_shader.instance_uniform_stride() * material.internal_id;
            buffer_info.range = m_shader.instance_uniform_size();

            VkWriteDescriptorSet& descriptor =
                descriptor_writes[descriptor_count++];
            descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor.dstSet = descriptor_set;
            descriptor.dstBinding = m_shader.instance_uniform_binding();
            descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor.descriptorCount = 1;
            descriptor.pBufferInfo = &buffer_info;
            uniform_generation = material.generation;
        }

        Texture* texture = material.diffuse_map.texture;
        if (texture == nullptr || !texture->valid())
            texture = m_default_texture;
        if (texture == nullptr || !texture->valid()) {
            ErrorLog("MaterialShader has no valid diffuse texture.");
            return;
        }

        DescriptorState& sampler_state =
            instance.descriptor_states[sampler_descriptor_index];
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

            VkWriteDescriptorSet& descriptor =
                descriptor_writes[descriptor_count++];
            descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor.dstSet = descriptor_set;
            descriptor.dstBinding = m_shader.instance_sampler_binding();
            descriptor.descriptorType =
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor.descriptorCount = 1;
            descriptor.pImageInfo = &image_info;
            sampler_generation = texture->generation;
            sampler_id = texture->id;
        }

        m_shader.update_descriptors(
            descriptor_count, descriptor_writes);
        m_shader.bind_instance_descriptor_set(
            command_buffer, descriptor_set);
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
        auto descriptors_allocated =
            m_shader.allocate_instance_descriptor_sets(
                instance.descriptor_sets);
        if (!descriptors_allocated)
            return err(descriptors_allocated.error());

        for (DescriptorState& state : instance.descriptor_states) {
            if (!state.generations.arr_init(m_allocator, m_image_count) ||
                !state.ids.arr_init(m_allocator, m_image_count)) {
                for (DescriptorState& initialized :
                     instance.descriptor_states) {
                    (void)initialized.generations.arr_shutdown();
                    (void)initialized.ids.arr_shutdown();
                }
                auto descriptors_released =
                    m_shader.release_instance_descriptor_sets(
                        instance.descriptor_sets);
                if (!descriptors_released) {
                    ErrorLog(
                        "Failed to roll back material descriptor sets: renderer_error={}, native_code={}.",
                        static_cast<u32>(descriptors_released.error().code),
                        descriptors_released.error().native_code);
                    return err(descriptors_released.error());
                }
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

        auto released = m_shader.release_instance_descriptor_sets(
            instance.descriptor_sets);
        if (!released) {
            ErrorLog(
                "Failed to free material descriptor sets: renderer_error={}, native_code={}.",
                static_cast<u32>(released.error().code),
                released.error().native_code);
            return;
        }
        for (DescriptorState& state : instance.descriptor_states) {
            (void)state.generations.arr_shutdown();
            (void)state.ids.arr_shutdown();
        }
        material.internal_id = numeric::invalid_id;
    }
}
