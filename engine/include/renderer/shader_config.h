#pragma once

#include "collections/slice.h"
#include "core/result.h"
#include "core/strview.h"

namespace nk {
    enum class ShaderStage : u8 {
        none = 0,
        vertex = 1 << 0,
        geometry = 1 << 1,
        fragment = 1 << 2,
        compute = 1 << 3,
    };

    constexpr ShaderStage operator|(
        const ShaderStage left,
        const ShaderStage right) noexcept {
        return static_cast<ShaderStage>(
            static_cast<u8>(left) | static_cast<u8>(right));
    }

    constexpr bool has_shader_stage(
        const ShaderStage stages,
        const ShaderStage stage) noexcept {
        return (static_cast<u8>(stages) & static_cast<u8>(stage)) != 0;
    }

    enum class ShaderAttributeType : u8 {
        f32,
        f32_2,
        f32_3,
        f32_4,
        i8,
        i8_2,
        i8_3,
        i8_4,
        u8,
        u8_2,
        u8_3,
        u8_4,
        i16,
        i16_2,
        i16_3,
        i16_4,
        u16,
        u16_2,
        u16_3,
        u16_4,
        i32,
        i32_2,
        i32_3,
        i32_4,
        u32,
        u32_2,
        u32_3,
        u32_4,
    };

    enum class ShaderUniformType : u8 {
        f32,
        f32_2,
        f32_3,
        f32_4,
        i8,
        u8,
        i16,
        u16,
        i32,
        u32,
        mat4,
        sampler_2d,
        custom,
    };

    enum class ShaderScope : u8 {
        global,
        instance,
        local,
    };

    enum class ShaderDescriptorType : u8 {
        uniform_buffer,
        sampler,
    };

    struct ShaderStageConfig {
        ShaderStage stage = ShaderStage::none;
        strview file_suffix;
        strview entry_point{"main"};
    };

    struct ShaderAttributeConfig {
        strview name;
        ShaderAttributeType type = ShaderAttributeType::f32;
        u32 location = 0;
        u32 offset = 0;
    };

    struct ShaderDescriptorBindingConfig {
        u32 binding = 0;
        ShaderDescriptorType type = ShaderDescriptorType::uniform_buffer;
        u32 count = 1;
        ShaderStage stages = ShaderStage::none;
        u64 element_size = 0;
    };

    struct ShaderDescriptorSetConfig {
        ShaderScope scope = ShaderScope::global;
        cl::slice<const ShaderDescriptorBindingConfig> bindings;
    };

    struct ShaderUniformConfig {
        strview name;
        ShaderUniformType type = ShaderUniformType::f32;
        ShaderScope scope = ShaderScope::global;
        u32 binding = 0;
        u32 offset = 0;
        u32 custom_size = 0;
    };

    struct ShaderPushConstantConfig {
        ShaderStage stages = ShaderStage::none;
        u32 offset = 0;
        u32 size = 0;
    };

    struct ShaderConfig {
        strview name;
        cl::slice<const ShaderStageConfig> stages;
        cl::slice<const ShaderAttributeConfig> attributes;
        cl::slice<const ShaderDescriptorSetConfig> descriptor_sets;
        cl::slice<const ShaderUniformConfig> uniforms;
        cl::slice<const ShaderPushConstantConfig> push_constants;
        u32 vertex_stride = 0;
        u32 max_instances = 0;
        bool wireframe = false;
        bool depth_test_enabled = true;
    };

    enum class shader_config_error : u8 {
        invalid_name,
        invalid_stage,
        duplicate_stage,
        invalid_vertex_layout,
        duplicate_attribute_location,
        invalid_descriptor_set,
        duplicate_descriptor_scope,
        invalid_descriptor_binding,
        duplicate_descriptor_binding,
        invalid_uniform,
        duplicate_uniform_name,
        invalid_push_constant,
        overlapping_push_constants,
        unsupported_layout,
        metadata_limits_exceeded,
    };

    [[nodiscard]] constexpr u32 shader_attribute_size(
        ShaderAttributeType type) noexcept;
    [[nodiscard]] constexpr u32 shader_uniform_size(
        const ShaderUniformConfig& uniform) noexcept;
    [[nodiscard]] result<void, shader_config_error> validate_shader_config(
        const ShaderConfig& config) noexcept;
}

constexpr nk::u32 nk::shader_attribute_size(
    const ShaderAttributeType type) noexcept {
    switch (type) {
        case ShaderAttributeType::f32:
        case ShaderAttributeType::i32:
        case ShaderAttributeType::u32:
            return 4;
        case ShaderAttributeType::f32_2:
        case ShaderAttributeType::i32_2:
        case ShaderAttributeType::u32_2:
            return 8;
        case ShaderAttributeType::f32_3:
        case ShaderAttributeType::i32_3:
        case ShaderAttributeType::u32_3:
            return 12;
        case ShaderAttributeType::f32_4:
        case ShaderAttributeType::i32_4:
        case ShaderAttributeType::u32_4:
            return 16;
        case ShaderAttributeType::i8:
        case ShaderAttributeType::u8:
            return 1;
        case ShaderAttributeType::i8_2:
        case ShaderAttributeType::u8_2:
            return 2;
        case ShaderAttributeType::i8_3:
        case ShaderAttributeType::u8_3:
            return 3;
        case ShaderAttributeType::i8_4:
        case ShaderAttributeType::u8_4:
            return 4;
        case ShaderAttributeType::i16:
        case ShaderAttributeType::u16:
            return 2;
        case ShaderAttributeType::i16_2:
        case ShaderAttributeType::u16_2:
            return 4;
        case ShaderAttributeType::i16_3:
        case ShaderAttributeType::u16_3:
            return 6;
        case ShaderAttributeType::i16_4:
        case ShaderAttributeType::u16_4:
            return 8;
    }
    return 0;
}

constexpr nk::u32 nk::shader_uniform_size(
    const ShaderUniformConfig& uniform) noexcept {
    switch (uniform.type) {
        case ShaderUniformType::f32:
        case ShaderUniformType::i32:
        case ShaderUniformType::u32:
            return 4;
        case ShaderUniformType::f32_2:
            return 8;
        case ShaderUniformType::f32_3:
            return 12;
        case ShaderUniformType::f32_4:
            return 16;
        case ShaderUniformType::i8:
        case ShaderUniformType::u8:
            return 1;
        case ShaderUniformType::i16:
        case ShaderUniformType::u16:
            return 2;
        case ShaderUniformType::mat4:
            return 64;
        case ShaderUniformType::sampler_2d:
            return 0;
        case ShaderUniformType::custom:
            return uniform.custom_size;
    }
    return 0;
}
