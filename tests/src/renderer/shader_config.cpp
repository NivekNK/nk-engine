#include <gtest/gtest.h>

#include "renderer/shader_config.h"

namespace {
    const nk::ShaderStageConfig stages[] = {
        {nk::ShaderStage::vertex, "vertex", "main"},
        {nk::ShaderStage::fragment, "fragment", "main"},
    };
    const nk::ShaderAttributeConfig attributes[] = {
        {"position", nk::ShaderAttributeType::f32_3, 0, 0},
        {"normal", nk::ShaderAttributeType::f32_3, 1, 12},
        {"texcoord", nk::ShaderAttributeType::f32_2, 2, 24},
    };
    const nk::ShaderDescriptorBindingConfig global_bindings[] = {
        {0, nk::ShaderDescriptorType::uniform_buffer, 1,
         nk::ShaderStage::vertex | nk::ShaderStage::fragment, 188},
    };
    const nk::ShaderDescriptorBindingConfig instance_bindings[] = {
        {0, nk::ShaderDescriptorType::uniform_buffer, 1,
         nk::ShaderStage::fragment, 64},
        {1, nk::ShaderDescriptorType::sampler, 2,
         nk::ShaderStage::fragment, 0},
    };
    const nk::ShaderDescriptorSetConfig descriptor_sets[] = {
        {nk::ShaderScope::global, global_bindings},
        {nk::ShaderScope::instance, instance_bindings},
    };
    const nk::ShaderUniformConfig uniforms[] = {
        {"projection", nk::ShaderUniformType::mat4,
         nk::ShaderScope::global, 0, 0, 0},
        {"view", nk::ShaderUniformType::mat4,
         nk::ShaderScope::global, 0, 64, 0},
        {"diffuse_color", nk::ShaderUniformType::f32_4,
         nk::ShaderScope::instance, 0, 0, 0},
        {"diffuse_texture", nk::ShaderUniformType::sampler_2d,
         nk::ShaderScope::instance, 1, 0, 0},
        {"model", nk::ShaderUniformType::mat4,
         nk::ShaderScope::local, 0, 0, 0},
        {"ambient_color", nk::ShaderUniformType::f32_4,
         nk::ShaderScope::global, 0, 128, 0},
        {"directional_light_direction", nk::ShaderUniformType::f32_3,
         nk::ShaderScope::global, 0, 144, 0},
        {"directional_light_color", nk::ShaderUniformType::f32_4,
         nk::ShaderScope::global, 0, 160, 0},
        {"normal_matrix", nk::ShaderUniformType::mat4,
         nk::ShaderScope::local, 0, 64, 0},
        {"specular_texture", nk::ShaderUniformType::sampler_2d,
         nk::ShaderScope::instance, 1, 1, 0},
        {"shininess", nk::ShaderUniformType::f32,
         nk::ShaderScope::instance, 0, 16, 0},
        {"view_position", nk::ShaderUniformType::f32_3,
         nk::ShaderScope::global, 0, 176, 0},
    };
    const nk::ShaderPushConstantConfig push_constants[] = {
        {nk::ShaderStage::vertex, 0, 128},
    };

    nk::ShaderConfig valid_config() {
        return {
            .name = "Builtin.MaterialShader",
            .stages = stages,
            .attributes = attributes,
            .descriptor_sets = descriptor_sets,
            .uniforms = uniforms,
            .push_constants = push_constants,
            .vertex_stride = 32,
            .max_instances = 1024,
            .wireframe = false,
            .depth_test_enabled = true,
        };
    }
}

TEST(ShaderConfig, AcceptsTypedMaterialLayout) {
    const auto validated = nk::validate_shader_config(valid_config());
    EXPECT_TRUE(validated);
}

TEST(ShaderConfig, RejectsDuplicateStagesAndAttributeLocations) {
    const nk::ShaderStageConfig duplicate_stages[] = {
        {nk::ShaderStage::vertex, "vertex", "main"},
        {nk::ShaderStage::vertex, "other", "main"},
    };
    nk::ShaderConfig config = valid_config();
    config.stages = duplicate_stages;
    auto stage_result = nk::validate_shader_config(config);
    ASSERT_FALSE(stage_result);
    EXPECT_EQ(stage_result.error(), nk::shader_config_error::duplicate_stage);

    const nk::ShaderAttributeConfig duplicate_attributes[] = {
        {"position", nk::ShaderAttributeType::f32_3, 0, 0},
        {"texcoord", nk::ShaderAttributeType::f32_2, 0, 12},
    };
    config = valid_config();
    config.attributes = duplicate_attributes;
    auto attribute_result = nk::validate_shader_config(config);
    ASSERT_FALSE(attribute_result);
    EXPECT_EQ(
        attribute_result.error(),
        nk::shader_config_error::duplicate_attribute_location);
}

TEST(ShaderConfig, RejectsOverlappingAttributes) {
    const nk::ShaderAttributeConfig overlapping_attributes[] = {
        {"position", nk::ShaderAttributeType::f32_3, 0, 0},
        {"texcoord", nk::ShaderAttributeType::f32_2, 1, 8},
    };
    nk::ShaderConfig config = valid_config();
    config.attributes = overlapping_attributes;

    const auto validated = nk::validate_shader_config(config);

    ASSERT_FALSE(validated);
    EXPECT_EQ(
        validated.error(),
        nk::shader_config_error::invalid_vertex_layout);
}

TEST(ShaderConfig, RejectsOutOfBoundsAttributesAndUniforms) {
    nk::ShaderConfig config = valid_config();
    config.vertex_stride = 16;
    auto attribute_result = nk::validate_shader_config(config);
    ASSERT_FALSE(attribute_result);
    EXPECT_EQ(
        attribute_result.error(),
        nk::shader_config_error::invalid_vertex_layout);

    const nk::ShaderUniformConfig invalid_uniforms[] = {
        {"outside", nk::ShaderUniformType::mat4,
         nk::ShaderScope::instance, 0, 16, 0},
    };
    config = valid_config();
    config.uniforms = invalid_uniforms;
    auto uniform_result = nk::validate_shader_config(config);
    ASSERT_FALSE(uniform_result);
    EXPECT_EQ(
        uniform_result.error(),
        nk::shader_config_error::invalid_uniform);
}

TEST(ShaderConfig, RejectsDescriptorScopeAndBindingConflicts) {
    const nk::ShaderDescriptorSetConfig duplicate_scopes[] = {
        {nk::ShaderScope::global, global_bindings},
        {nk::ShaderScope::global, global_bindings},
    };
    nk::ShaderConfig config = valid_config();
    config.descriptor_sets = duplicate_scopes;
    auto scope_result = nk::validate_shader_config(config);
    ASSERT_FALSE(scope_result);
    EXPECT_EQ(
        scope_result.error(),
        nk::shader_config_error::duplicate_descriptor_scope);

    const nk::ShaderDescriptorBindingConfig duplicate_bindings[] = {
        {0, nk::ShaderDescriptorType::uniform_buffer, 1,
         nk::ShaderStage::fragment, 64},
        {0, nk::ShaderDescriptorType::sampler, 1,
         nk::ShaderStage::fragment, 0},
    };
    const nk::ShaderDescriptorSetConfig invalid_sets[] = {
        {nk::ShaderScope::global, global_bindings},
        {nk::ShaderScope::instance, duplicate_bindings},
    };
    config = valid_config();
    config.descriptor_sets = invalid_sets;
    auto binding_result = nk::validate_shader_config(config);
    ASSERT_FALSE(binding_result);
    EXPECT_EQ(
        binding_result.error(),
        nk::shader_config_error::duplicate_descriptor_binding);
}

TEST(ShaderConfig, RejectsBindingsForMissingStages) {
    const nk::ShaderDescriptorBindingConfig invalid_global_bindings[] = {
        {0, nk::ShaderDescriptorType::uniform_buffer, 1,
         nk::ShaderStage::geometry, 256},
    };
    const nk::ShaderDescriptorSetConfig invalid_sets[] = {
        {nk::ShaderScope::global, invalid_global_bindings},
        {nk::ShaderScope::instance, instance_bindings},
    };
    nk::ShaderConfig config = valid_config();
    config.descriptor_sets = invalid_sets;

    auto descriptor_result = nk::validate_shader_config(config);
    ASSERT_FALSE(descriptor_result);
    EXPECT_EQ(
        descriptor_result.error(),
        nk::shader_config_error::invalid_descriptor_binding);

    const nk::ShaderPushConstantConfig invalid_push_constants[] = {
        {nk::ShaderStage::geometry, 0, 128},
    };
    config = valid_config();
    config.push_constants = invalid_push_constants;
    auto push_result = nk::validate_shader_config(config);
    ASSERT_FALSE(push_result);
    EXPECT_EQ(
        push_result.error(),
        nk::shader_config_error::invalid_push_constant);
}

TEST(ShaderConfig, RequiresLocalUniformsInsidePushConstantRanges) {
    nk::ShaderConfig config = valid_config();
    config.push_constants = {};
    auto missing = nk::validate_shader_config(config);
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nk::shader_config_error::invalid_uniform);

    const nk::ShaderPushConstantConfig overlapping[] = {
        {nk::ShaderStage::vertex, 0, 128},
        {nk::ShaderStage::fragment, 32, 16},
    };
    config = valid_config();
    config.push_constants = overlapping;
    auto overlap = nk::validate_shader_config(config);
    ASSERT_FALSE(overlap);
    EXPECT_EQ(
        overlap.error(),
        nk::shader_config_error::overlapping_push_constants);
}

TEST(ShaderConfig, ReportsAllAttributeAndUniformSizes) {
    EXPECT_EQ(
        nk::shader_attribute_size(nk::ShaderAttributeType::i8_3), 3);
    EXPECT_EQ(
        nk::shader_attribute_size(nk::ShaderAttributeType::u16_4), 8);
    EXPECT_EQ(
        nk::shader_attribute_size(nk::ShaderAttributeType::f32_4), 16);

    nk::ShaderUniformConfig custom{
        .name = "payload",
        .type = nk::ShaderUniformType::custom,
        .custom_size = 48,
    };
    EXPECT_EQ(nk::shader_uniform_size(custom), 48);
}
