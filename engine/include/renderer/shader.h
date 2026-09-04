#pragma once

#include <concepts>
#include <type_traits>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

#include "collections/arr.h"
#include "core/result.h"
#include "renderer/renderer_result.h"
#include "renderer/shader_config.h"

namespace nk {
    struct ShaderHandle {
        u16 index = numeric::u16_max;
        u16 generation = numeric::u16_max;

        [[nodiscard]] constexpr bool valid() const noexcept {
            return index != numeric::u16_max &&
                   generation != numeric::u16_max;
        }

        constexpr bool operator==(const ShaderHandle&) const noexcept = default;
    };

    struct ShaderUniformHandle {
        u16 index = numeric::u16_max;

        [[nodiscard]] constexpr bool valid() const noexcept {
            return index != numeric::u16_max;
        }

        constexpr bool operator==(
            const ShaderUniformHandle&) const noexcept = default;
    };

    struct ShaderUniformMetadata {
        u16 offset = 0;
        u16 size = 0;
        u8 binding = 0;
        u8 array_length = 1;
        ShaderUniformType type = ShaderUniformType::f32;
        ShaderScope scope = ShaderScope::global;
    };

    class ShaderMetadata {
    public:
        ShaderMetadata() = default;
        ~ShaderMetadata() { shutdown(); }

        ShaderMetadata(const ShaderMetadata&) = delete;
        ShaderMetadata& operator=(const ShaderMetadata&) = delete;
        ShaderMetadata(ShaderMetadata&&) = delete;
        ShaderMetadata& operator=(ShaderMetadata&&) = delete;

        [[nodiscard]] result<void, renderer_error> init(
            mem::Allocator* allocator,
            const ShaderConfig& config);
        void shutdown() noexcept;

        [[nodiscard]] const ShaderUniformMetadata* uniform(
            ShaderUniformHandle handle) const noexcept;
        [[nodiscard]] u16 uniform_count() const noexcept {
            return static_cast<u16>(m_uniforms.length());
        }
        [[nodiscard]] bool initialized() const noexcept {
            return m_uniforms.allocator() != nullptr;
        }

    private:
        cl::arr<ShaderUniformMetadata> m_uniforms;
    };

    template <typename>
    inline constexpr bool is_shader_uniform_value = false;

    template <> inline constexpr bool is_shader_uniform_value<f32> = true;
    template <> inline constexpr bool is_shader_uniform_value<i8> = true;
    template <> inline constexpr bool is_shader_uniform_value<u8> = true;
    template <> inline constexpr bool is_shader_uniform_value<i16> = true;
    template <> inline constexpr bool is_shader_uniform_value<u16> = true;
    template <> inline constexpr bool is_shader_uniform_value<i32> = true;
    template <> inline constexpr bool is_shader_uniform_value<u32> = true;
    template <> inline constexpr bool is_shader_uniform_value<glm::vec2> = true;
    template <> inline constexpr bool is_shader_uniform_value<glm::vec3> = true;
    template <> inline constexpr bool is_shader_uniform_value<glm::vec4> = true;
    template <> inline constexpr bool is_shader_uniform_value<glm::mat4> = true;

    template <typename T>
    concept ShaderUniformValue =
        is_shader_uniform_value<std::remove_cvref_t<T>>;

    template <ShaderUniformValue T>
    [[nodiscard]] constexpr ShaderUniformType shader_uniform_type() noexcept {
        using Value = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Value, f32>)
            return ShaderUniformType::f32;
        else if constexpr (std::same_as<Value, glm::vec2>)
            return ShaderUniformType::f32_2;
        else if constexpr (std::same_as<Value, glm::vec3>)
            return ShaderUniformType::f32_3;
        else if constexpr (std::same_as<Value, glm::vec4>)
            return ShaderUniformType::f32_4;
        else if constexpr (std::same_as<Value, i8>)
            return ShaderUniformType::i8;
        else if constexpr (std::same_as<Value, u8>)
            return ShaderUniformType::u8;
        else if constexpr (std::same_as<Value, i16>)
            return ShaderUniformType::i16;
        else if constexpr (std::same_as<Value, u16>)
            return ShaderUniformType::u16;
        else if constexpr (std::same_as<Value, i32>)
            return ShaderUniformType::i32;
        else if constexpr (std::same_as<Value, u32>)
            return ShaderUniformType::u32;
        else
            return ShaderUniformType::mat4;
    }

    namespace builtin_shader_uniform {
        inline constexpr ShaderUniformHandle projection{0};
        inline constexpr ShaderUniformHandle view{1};
        inline constexpr ShaderUniformHandle diffuse_color{2};
        inline constexpr ShaderUniformHandle diffuse_texture{3};
        inline constexpr ShaderUniformHandle model{4};
        inline constexpr ShaderUniformHandle ambient_color{5};
        inline constexpr ShaderUniformHandle directional_light_direction{6};
        inline constexpr ShaderUniformHandle directional_light_color{7};
        inline constexpr ShaderUniformHandle normal_matrix{8};
        inline constexpr ShaderUniformHandle specular_texture{9};
        inline constexpr ShaderUniformHandle shininess{10};
        inline constexpr ShaderUniformHandle view_position{11};
        inline constexpr ShaderUniformHandle normal_texture{12};
        inline constexpr ShaderUniformHandle point_light_count{13};
        inline constexpr ShaderUniformHandle point_lights{14};
        inline constexpr ShaderUniformHandle render_view_mode{15};
    }
}

static_assert(sizeof(nk::ShaderHandle) == 4);
static_assert(sizeof(nk::ShaderUniformHandle) == 2);
static_assert(sizeof(nk::ShaderUniformMetadata) == 8);
