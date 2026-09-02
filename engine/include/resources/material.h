#pragma once

#include <glm/ext/vector_float4.hpp>

#include "core/strbuf.h"
#include "resources/texture.h"

namespace nk {
    inline constexpr u64 material_name_capacity = 255;
    inline constexpr u64 texture_name_capacity = 255;

    enum class TextureUse : u8 {
        unknown,
        diffuse,
    };

    struct TextureMap {
        Texture* texture = nullptr;
        TextureUse use = TextureUse::unknown;
    };

    struct MaterialConfig {
        strbuf<material_name_capacity> name;
        bool auto_release = true;
        glm::vec4 diffuse_color{1.0f};
        strbuf<texture_name_capacity> diffuse_map_name;
    };

    struct Material {
        u32 id = numeric::invalid_id;
        u32 generation = numeric::invalid_id;
        u32 internal_id = numeric::invalid_id;
        strbuf<material_name_capacity> name;
        glm::vec4 diffuse_color{1.0f};
        TextureMap diffuse_map{};
        strbuf<texture_name_capacity> diffuse_map_name;

        bool valid() const noexcept {
            return generation != numeric::invalid_id &&
                   internal_id != numeric::invalid_id;
        }
    };
}
