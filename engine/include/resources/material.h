#pragma once

#include <glm/ext/vector_float4.hpp>

#include "core/strbuf.h"
#include "renderer/shader.h"
#include "renderer/sampler.h"
#include "resources/texture.h"

namespace nk {
    inline constexpr u64 material_name_capacity = 255;
    inline constexpr u64 texture_name_capacity = 255;
    inline constexpr u64 shader_name_capacity = 127;

    enum class TextureUse : u8 {
        unknown = 0,
        diffuse = 1,
        specular = 2,
        normal = 3,
    };

    enum class MaterialType : u8 {
        world,
        ui,
    };

    struct TextureMap {
        Texture* texture = nullptr;
        TextureUse use = TextureUse::unknown;
        SamplerConfig sampling{};
        SamplerHandle sampler{};

        TextureBinding binding() const noexcept { return {texture, sampler}; }
    };

    struct MaterialConfig {
        strbuf<material_name_capacity> name;
        strbuf<shader_name_capacity> shader_name;
        bool auto_release = true;
        MaterialType type = MaterialType::world;
        glm::vec4 diffuse_color{1.0f};
        strbuf<texture_name_capacity> diffuse_map_name;
        strbuf<texture_name_capacity> specular_map_name;
        strbuf<texture_name_capacity> normal_map_name;
        f32 shininess = 32.0f;
        SamplerConfig diffuse_sampler{};
        SamplerConfig specular_sampler{};
        SamplerConfig normal_sampler{};
    };

    struct MaterialApplyState {
        u64 frame_number = numeric::u64_max;
        u32 material_generation = numeric::invalid_id;
        u32 diffuse_texture_id = numeric::invalid_id;
        u32 diffuse_texture_generation = numeric::invalid_id;
        u32 specular_texture_id = numeric::invalid_id;
        u32 specular_texture_generation = numeric::invalid_id;
        u32 normal_texture_id = numeric::invalid_id;
        u32 normal_texture_generation = numeric::invalid_id;
        u32 instance_id = numeric::invalid_id;
        ShaderHandle shader{};
    };

    struct Material {
        u32 id = numeric::invalid_id;
        u32 generation = numeric::invalid_id;
        u32 internal_id = numeric::invalid_id;
        strbuf<material_name_capacity> name;
        ShaderHandle shader{};
        MaterialType type = MaterialType::world;
        glm::vec4 diffuse_color{1.0f};
        TextureMap diffuse_map{};
        strbuf<texture_name_capacity> diffuse_map_name;
        TextureMap specular_map{};
        strbuf<texture_name_capacity> specular_map_name;
        TextureMap normal_map{};
        strbuf<texture_name_capacity> normal_map_name;
        f32 shininess = 32.0f;
        MaterialApplyState apply_state{};

        bool valid() const noexcept {
            return generation != numeric::invalid_id &&
                   internal_id != numeric::invalid_id && shader.valid();
        }
    };
}
