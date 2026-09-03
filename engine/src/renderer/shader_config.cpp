#include "nkpch.h"

#include "renderer/shader_config.h"

namespace nk {
    namespace {
        constexpr ShaderStage graphics_stages =
            ShaderStage::vertex |
            ShaderStage::geometry |
            ShaderStage::fragment;

        bool is_single_graphics_stage(const ShaderStage stage) noexcept {
            const u8 bits = static_cast<u8>(stage);
            return bits != 0 && (bits & (bits - 1)) == 0 &&
                (bits & ~static_cast<u8>(graphics_stages)) == 0;
        }

        bool valid_graphics_stages(const ShaderStage stages) noexcept {
            const u8 bits = static_cast<u8>(stages);
            return bits != 0 &&
                (bits & ~static_cast<u8>(graphics_stages)) == 0;
        }

        const ShaderDescriptorSetConfig* find_set(
            const ShaderConfig& config,
            const ShaderScope scope) noexcept {
            for (const ShaderDescriptorSetConfig& set : config.descriptor_sets) {
                if (set.scope == scope)
                    return &set;
            }
            return nullptr;
        }

        const ShaderDescriptorBindingConfig* find_binding(
            const ShaderDescriptorSetConfig& set,
            const u32 binding) noexcept {
            for (const ShaderDescriptorBindingConfig& candidate : set.bindings) {
                if (candidate.binding == binding)
                    return &candidate;
            }
            return nullptr;
        }

        bool ranges_overlap(
            const u32 first_offset,
            const u32 first_size,
            const u32 second_offset,
            const u32 second_size) noexcept {
            return first_offset < second_offset + second_size &&
                second_offset < first_offset + first_size;
        }
    }

    result<void, shader_config_error> validate_shader_config(
        const ShaderConfig& config) noexcept {
        if (config.name.empty())
            return err(shader_config_error::invalid_name);
        if (config.stages.empty())
            return err(shader_config_error::invalid_stage);

        ShaderStage configured_stages = ShaderStage::none;
        for (const ShaderStageConfig& stage : config.stages) {
            if (!is_single_graphics_stage(stage.stage) ||
                stage.file_suffix.empty() || stage.entry_point.empty()) {
                return err(shader_config_error::invalid_stage);
            }
            if (has_shader_stage(configured_stages, stage.stage))
                return err(shader_config_error::duplicate_stage);
            configured_stages = configured_stages | stage.stage;
        }
        if (!has_shader_stage(configured_stages, ShaderStage::vertex) ||
            !has_shader_stage(configured_stages, ShaderStage::fragment)) {
            return err(shader_config_error::unsupported_layout);
        }

        if (config.attributes.empty() || config.vertex_stride == 0)
            return err(shader_config_error::invalid_vertex_layout);
        for (u64 index = 0; index < config.attributes.length(); ++index) {
            const ShaderAttributeConfig& attribute = config.attributes[index];
            const u32 size = shader_attribute_size(attribute.type);
            if (attribute.name.empty() || size == 0 ||
                attribute.offset > config.vertex_stride ||
                size > config.vertex_stride - attribute.offset) {
                return err(shader_config_error::invalid_vertex_layout);
            }
            for (u64 previous = 0; previous < index; ++previous) {
                if (config.attributes[previous].location == attribute.location) {
                    return err(
                        shader_config_error::duplicate_attribute_location);
                }
            }
        }

        if (config.descriptor_sets.empty() ||
            config.descriptor_sets.length() > 2) {
            return err(shader_config_error::unsupported_layout);
        }
        for (u64 set_index = 0;
             set_index < config.descriptor_sets.length();
             ++set_index) {
            const ShaderDescriptorSetConfig& set =
                config.descriptor_sets[set_index];
            if (set.scope == ShaderScope::local || set.bindings.empty())
                return err(shader_config_error::invalid_descriptor_set);
            if ((set.scope == ShaderScope::global && set_index != 0) ||
                (set.scope == ShaderScope::instance && set_index != 1)) {
                return err(shader_config_error::unsupported_layout);
            }
            for (u64 previous = 0; previous < set_index; ++previous) {
                if (config.descriptor_sets[previous].scope == set.scope) {
                    return err(
                        shader_config_error::duplicate_descriptor_scope);
                }
            }

            u32 uniform_buffer_count = 0;
            for (u64 binding_index = 0;
                 binding_index < set.bindings.length();
                 ++binding_index) {
                const ShaderDescriptorBindingConfig& binding =
                    set.bindings[binding_index];
                if (binding.count == 0 ||
                    !valid_graphics_stages(binding.stages) ||
                    (binding.type == ShaderDescriptorType::uniform_buffer &&
                     (binding.count != 1 || binding.element_size == 0)) ||
                    (binding.type == ShaderDescriptorType::sampler &&
                     binding.element_size != 0)) {
                    return err(
                        shader_config_error::invalid_descriptor_binding);
                }
                if (binding.type == ShaderDescriptorType::uniform_buffer &&
                    ++uniform_buffer_count > 1) {
                    return err(shader_config_error::unsupported_layout);
                }
                for (u64 previous = 0; previous < binding_index; ++previous) {
                    if (set.bindings[previous].binding == binding.binding) {
                        return err(
                            shader_config_error::duplicate_descriptor_binding);
                    }
                }
            }
        }

        const ShaderDescriptorSetConfig* instance_set =
            find_set(config, ShaderScope::instance);
        if ((instance_set == nullptr) != (config.max_instances == 0))
            return err(shader_config_error::invalid_descriptor_set);

        for (u64 index = 0; index < config.uniforms.length(); ++index) {
            const ShaderUniformConfig& uniform = config.uniforms[index];
            if (uniform.name.empty())
                return err(shader_config_error::invalid_uniform);
            for (u64 previous = 0; previous < index; ++previous) {
                if (config.uniforms[previous].name == uniform.name)
                    return err(shader_config_error::duplicate_uniform_name);
            }

            const u32 uniform_size = shader_uniform_size(uniform);
            if (uniform.type == ShaderUniformType::custom &&
                uniform_size == 0) {
                return err(shader_config_error::invalid_uniform);
            }
            if (uniform.type != ShaderUniformType::custom &&
                uniform.custom_size != 0) {
                return err(shader_config_error::invalid_uniform);
            }

            if (uniform.scope == ShaderScope::local) {
                if (uniform.type == ShaderUniformType::sampler_2d ||
                    uniform_size == 0) {
                    return err(shader_config_error::invalid_uniform);
                }
                bool covered = false;
                for (const ShaderPushConstantConfig& range :
                     config.push_constants) {
                    if (uniform.offset >= range.offset &&
                        uniform.offset - range.offset <= range.size &&
                        uniform_size <=
                            range.size - (uniform.offset - range.offset)) {
                        covered = true;
                        break;
                    }
                }
                if (!covered)
                    return err(shader_config_error::invalid_uniform);
                continue;
            }

            const ShaderDescriptorSetConfig* set =
                find_set(config, uniform.scope);
            if (set == nullptr)
                return err(shader_config_error::invalid_uniform);
            const ShaderDescriptorBindingConfig* binding =
                find_binding(*set, uniform.binding);
            if (binding == nullptr)
                return err(shader_config_error::invalid_uniform);

            if (uniform.type == ShaderUniformType::sampler_2d) {
                if (binding->type != ShaderDescriptorType::sampler ||
                    uniform.offset >= binding->count) {
                    return err(shader_config_error::invalid_uniform);
                }
            } else if (binding->type !=
                           ShaderDescriptorType::uniform_buffer ||
                       uniform.offset > binding->element_size ||
                       uniform_size > binding->element_size - uniform.offset) {
                return err(shader_config_error::invalid_uniform);
            }
        }

        for (u64 index = 0;
             index < config.push_constants.length();
             ++index) {
            const ShaderPushConstantConfig& range =
                config.push_constants[index];
            if (!valid_graphics_stages(range.stages) || range.size == 0 ||
                (range.offset % 4) != 0 || (range.size % 4) != 0 ||
                range.offset > numeric::u32_max - range.size) {
                return err(shader_config_error::invalid_push_constant);
            }
            for (u64 previous = 0; previous < index; ++previous) {
                const ShaderPushConstantConfig& other =
                    config.push_constants[previous];
                if (ranges_overlap(
                        range.offset,
                        range.size,
                        other.offset,
                        other.size)) {
                    return err(
                        shader_config_error::overlapping_push_constants);
                }
            }
        }

        return ok();
    }
}
