#include "nkpch.h"

#include "vulkan/shaders/material_shader.h"

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

        renderer_error invalid_shader_state() noexcept {
            return {renderer_error_code::shader_state_invalid, 0};
        }

        renderer_error invalid_uniform() noexcept {
            return {renderer_error_code::shader_uniform_invalid, 0};
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
            config.max_instances == 0 ||
            config.max_instances > max_material_count ||
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
        m_max_instances = config.max_instances;

        auto metadata_initialized = m_metadata.init(allocator, config);
        if (!metadata_initialized) {
            shutdown();
            return err(metadata_initialized.error());
        }

        auto shader_initialized = m_shader.init(
            config,
            width,
            height,
            image_count,
            render_pass,
            device,
            allocator,
            resources,
            vulkan_allocator);
        if (!shader_initialized) {
            shutdown();
            return err(shader_initialized.error());
        }

        if (m_shader.global_uniform_size() == 0 ||
            m_shader.instance_uniform_binding() == numeric::invalid_id ||
            m_shader.instance_sampler_binding() == numeric::invalid_id ||
            m_shader.instance_uniform_size() == 0 ||
            m_shader.instance_uniform_size() > numeric::u16_max) {
            shutdown();
            return err(invalid_material_shader_config());
        }

        m_instance_uniform_size = m_shader.instance_uniform_size();
        if (m_instance_uniform_size >
                numeric::u64_max / m_max_instances ||
            !m_global_uniform_data.arr_init(
                allocator, m_shader.global_uniform_size()) ||
            !m_instance_uniform_data.arr_init(
                allocator, m_instance_uniform_size * m_max_instances)) {
            shutdown();
            return err(renderer_error{
                renderer_error_code::out_of_memory,
                0,
            });
        }

        return ok();
    }

    void MaterialShader::shutdown() {
        for (MaterialShaderInstanceState& instance : m_instance_states) {
            if (instance.descriptor_sets.allocator() != nullptr)
                release_instance_metadata(instance);
        }
        (void)m_instance_uniform_data.arr_shutdown();
        (void)m_global_uniform_data.arr_shutdown();
        m_shader.shutdown();
        m_metadata.shutdown();
        m_allocator = nullptr;
        m_default_texture = nullptr;
        m_image_count = 0;
        m_max_instances = 0;
        m_bound_instance_id = numeric::invalid_id;
        m_instance_uniform_size = 0;
        m_globals_bound = false;
        for (Texture*& texture : m_instance_textures)
            texture = nullptr;
    }

    result<void, renderer_error> MaterialShader::use(
        CommandBuffer& command_buffer) {
        if (!m_shader.initialized())
            return err(invalid_shader_state());
        m_shader.use(command_buffer);
        m_globals_bound = false;
        m_bound_instance_id = numeric::invalid_id;
        return ok();
    }

    result<void, renderer_error> MaterialShader::bind_globals() {
        if (!m_shader.initialized())
            return err(invalid_shader_state());
        m_globals_bound = true;
        m_bound_instance_id = numeric::invalid_id;
        return ok();
    }

    result<void, renderer_error> MaterialShader::bind_instance(
        const u32 instance_id) {
        if (!m_shader.initialized() || instance_id >= m_max_instances ||
            m_instance_states[instance_id].descriptor_sets.allocator() ==
                nullptr) {
            return err(invalid_shader_state());
        }
        m_bound_instance_id = instance_id;
        return ok();
    }

    result<void, renderer_error> MaterialShader::apply_globals(
        CommandBuffer& command_buffer,
        const u32 image_index) {
        if (!m_globals_bound)
            return err(invalid_shader_state());
        return m_shader.apply_global_uniform(
            command_buffer,
            image_index,
            m_global_uniform_data.data(),
            m_global_uniform_data.length());
    }

    result<void, renderer_error> MaterialShader::set_uniform(
        CommandBuffer& command_buffer,
        const ShaderUniformHandle uniform_handle,
        const ShaderUniformType type,
        const void* data,
        const u32 size) {
        const ShaderUniformMetadata* uniform =
            m_metadata.uniform(uniform_handle);
        if (uniform == nullptr || data == nullptr || size == 0 ||
            uniform->type != type || uniform->size != size ||
            type == ShaderUniformType::sampler_2d) {
            return err(invalid_uniform());
        }

        switch (uniform->scope) {
            case ShaderScope::global:
                if (!m_globals_bound ||
                    uniform->offset > m_global_uniform_data.length() ||
                    uniform->size >
                        m_global_uniform_data.length() - uniform->offset) {
                    return err(invalid_shader_state());
                }
                std::memcpy(
                    m_global_uniform_data.data() + uniform->offset,
                    data,
                    size);
                return ok();
            case ShaderScope::instance: {
                if (m_bound_instance_id >= m_max_instances)
                    return err(invalid_shader_state());
                const u64 base =
                    m_instance_uniform_size * m_bound_instance_id;
                if (uniform->offset > m_instance_uniform_size ||
                    uniform->size >
                        m_instance_uniform_size - uniform->offset) {
                    return err(invalid_shader_state());
                }
                std::memcpy(
                    m_instance_uniform_data.data() + base + uniform->offset,
                    data,
                    size);
                return ok();
            }
            case ShaderScope::local:
                return m_shader.push_constant(
                    command_buffer,
                    uniform->offset,
                    uniform->size,
                    data);
        }
        return err(invalid_uniform());
    }

    result<void, renderer_error> MaterialShader::set_sampler(
        const ShaderUniformHandle uniform_handle,
        Texture* texture) {
        const ShaderUniformMetadata* uniform =
            m_metadata.uniform(uniform_handle);
        if (uniform == nullptr ||
            uniform->type != ShaderUniformType::sampler_2d ||
            uniform->scope != ShaderScope::instance) {
            return err(invalid_uniform());
        }
        if (m_bound_instance_id >= m_max_instances)
            return err(invalid_shader_state());
        m_instance_textures[m_bound_instance_id] = texture;
        return ok();
    }

    result<void, renderer_error> MaterialShader::apply_instance(
        CommandBuffer& command_buffer,
        const u32 image_index) {
        if (m_bound_instance_id >= m_max_instances ||
            image_index >= m_image_count) {
            return err(invalid_shader_state());
        }

        MaterialShaderInstanceState& instance =
            m_instance_states[m_bound_instance_id];
        if (image_index >= instance.descriptor_sets.length())
            return err(invalid_shader_state());

        auto loaded = m_shader.load_instance_uniform(
            m_bound_instance_id,
            m_instance_uniform_data.data() +
                m_instance_uniform_size * m_bound_instance_id,
            m_instance_uniform_size);
        if (!loaded)
            return err(loaded.error());

        const VkDescriptorSet descriptor_set =
            instance.descriptor_sets[image_index];
        VkWriteDescriptorSet descriptor_writes[
            MaterialShaderInstanceState::descriptor_count]{};
        u32 descriptor_count = 0;

        VkDescriptorBufferInfo buffer_info{};
        u32& uniform_generation =
            instance.descriptor_states[uniform_descriptor_index]
                .generations[image_index];
        if (uniform_generation == numeric::invalid_id) {
            buffer_info.buffer = m_shader.instance_uniform_buffer();
            buffer_info.offset =
                m_shader.instance_uniform_stride() * m_bound_instance_id;
            buffer_info.range = m_shader.instance_uniform_size();

            VkWriteDescriptorSet& descriptor =
                descriptor_writes[descriptor_count++];
            descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor.dstSet = descriptor_set;
            descriptor.dstBinding = m_shader.instance_uniform_binding();
            descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor.descriptorCount = 1;
            descriptor.pBufferInfo = &buffer_info;
            uniform_generation = 0;
        }

        Texture* texture = m_instance_textures[m_bound_instance_id];
        if (texture == nullptr || !texture->valid())
            texture = m_default_texture;
        if (texture == nullptr || !texture->valid())
            return err(invalid_shader_state());

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

        m_shader.update_descriptors(descriptor_count, descriptor_writes);
        m_shader.bind_instance_descriptor_set(command_buffer, descriptor_set);
        return ok();
    }

    result<u32, renderer_error> MaterialShader::acquire_resources() {
        u32 instance_id = numeric::invalid_id;
        for (u32 index = 0; index < m_max_instances; ++index) {
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
                if (!descriptors_released)
                    return err(descriptors_released.error());
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

        std::memset(
            m_instance_uniform_data.data() +
                m_instance_uniform_size * instance_id,
            0,
            m_instance_uniform_size);
        m_instance_textures[instance_id] = nullptr;
        return ok(instance_id);
    }

    result<void, renderer_error> MaterialShader::release_resources(
        const u32 instance_id) {
        if (instance_id >= m_max_instances)
            return err(invalid_shader_state());
        MaterialShaderInstanceState& instance = m_instance_states[instance_id];
        if (instance.descriptor_sets.allocator() == nullptr)
            return err(invalid_shader_state());

        auto released = m_shader.release_instance_descriptor_sets(
            instance.descriptor_sets);
        if (!released)
            return err(released.error());
        for (DescriptorState& state : instance.descriptor_states) {
            (void)state.generations.arr_shutdown();
            (void)state.ids.arr_shutdown();
        }
        m_instance_textures[instance_id] = nullptr;
        if (m_bound_instance_id == instance_id)
            m_bound_instance_id = numeric::invalid_id;
        return ok();
    }
}
