#pragma once

#include "core/strbuf.h"
#include "renderer/shader_config.h"

namespace nk {
    class ShaderResourceLoader;

    enum class shader_resource_parse_error : i32 {
        invalid_syntax = 1,
        unsupported_version,
        duplicate_property,
        missing_property,
        invalid_name,
        invalid_render_pass,
        invalid_stage,
        stage_capacity_exceeded,
        invalid_attribute,
        attribute_capacity_exceeded,
        invalid_uniform,
        uniform_capacity_exceeded,
        invalid_number,
        invalid_boolean,
        invalid_layout,
    };

    class ShaderResourceConfig final {
    public:
        static constexpr u32 format_version = 1;
        static constexpr u32 max_stage_count = 4;
        static constexpr u32 max_attribute_count = 16;
        static constexpr u32 max_uniform_count = 64;

        ShaderResourceConfig() = default;
        ShaderResourceConfig(const ShaderResourceConfig&) = delete;
        ShaderResourceConfig& operator=(const ShaderResourceConfig&) = delete;
        ShaderResourceConfig(ShaderResourceConfig&&) = delete;
        ShaderResourceConfig& operator=(ShaderResourceConfig&&) = delete;

        [[nodiscard]] ShaderConfig config() const noexcept {
            return {
                .name = m_name.view(),
                .stages = {m_stages, m_stage_count},
                .attributes = {m_attributes, m_attribute_count},
                .descriptor_sets = {
                    m_descriptor_sets,
                    m_descriptor_set_count,
                },
                .uniforms = {m_uniforms, m_uniform_count},
                .push_constants = {
                    m_push_constants,
                    m_push_constant_count,
                },
                .vertex_stride = m_vertex_stride,
                .max_instances = m_max_instances,
                .wireframe = m_wireframe,
                .depth_test_enabled = m_depth_test_enabled,
                .depth_write_enabled = m_depth_write_enabled,
                .cull_mode = m_cull_mode,
                .depth_compare = m_depth_compare,
            };
        }

        [[nodiscard]] RenderPassKind render_pass() const noexcept {
            return m_render_pass;
        }

    private:
        static constexpr u32 max_binding_count = 2;

        strbuf<127> m_name;
        RenderPassKind m_render_pass = RenderPassKind::world;
        ShaderStageConfig m_stages[max_stage_count]{};
        strbuf<63> m_stage_suffixes[max_stage_count]{};
        strbuf<63> m_stage_entry_points[max_stage_count]{};
        ShaderAttributeConfig m_attributes[max_attribute_count]{};
        strbuf<63> m_attribute_names[max_attribute_count]{};
        ShaderUniformConfig m_uniforms[max_uniform_count]{};
        strbuf<63> m_uniform_names[max_uniform_count]{};
        ShaderDescriptorBindingConfig m_global_bindings[max_binding_count]{};
        ShaderDescriptorBindingConfig m_instance_bindings[max_binding_count]{};
        ShaderDescriptorSetConfig m_descriptor_sets[2]{};
        ShaderPushConstantConfig m_push_constants[1]{};
        u32 m_stage_count = 0;
        u32 m_attribute_count = 0;
        u32 m_uniform_count = 0;
        u32 m_descriptor_set_count = 0;
        u32 m_push_constant_count = 0;
        u32 m_vertex_stride = 0;
        u32 m_max_instances = 0;
        bool m_wireframe = false;
        bool m_depth_test_enabled = true;
        bool m_depth_write_enabled = true;
        ShaderCullMode m_cull_mode = ShaderCullMode::back;
        ShaderDepthCompare m_depth_compare = ShaderDepthCompare::less;

        friend class ShaderResourceLoader;
    };
}
