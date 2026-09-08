#pragma once

#include "collections/arr.h"
#include "collections/map.h"
#include "core/result.h"
#include "core/str.h"
#include "renderer/lighting.h"
#include "resources/material.h"

#include <glm/ext/matrix_float4x4.hpp>

namespace nk {
    class ResourceSystem;
    class ShaderSystem;
    class TextureSystem;
    namespace mem { class Allocator; }

    inline constexpr strview default_material_name{"default", 7};
    inline constexpr strview default_ui_material_name{"default_ui", 10};

    enum class material_error_code : u8 {
        invalid_name,
        not_initialized,
        capacity_exceeded,
        out_of_memory,
        file_failed,
        invalid_config,
        resource_failed,
        texture_failed,
        shader_failed,
    };

    struct material_error {
        material_error_code code;
        i32 native_code;
    };

    class MaterialSystem final {
    public:
        static constexpr u32 default_max_material_count = 1024;

        MaterialSystem() = default;
        ~MaterialSystem();

        MaterialSystem(const MaterialSystem&) = delete;
        MaterialSystem& operator=(const MaterialSystem&) = delete;
        MaterialSystem(MaterialSystem&&) = delete;
        MaterialSystem& operator=(MaterialSystem&&) = delete;

        [[nodiscard]] static result<MaterialSystem*, material_error> create(
            mem::Allocator& allocator,
            ShaderSystem& shaders,
            TextureSystem& textures,
            ResourceSystem& resources,
            u32 max_material_count = default_max_material_count);
        static void destroy(mem::Allocator& allocator, MaterialSystem* system);

        [[nodiscard]] result<Material*, material_error> acquire(strview name);
        [[nodiscard]] result<Material*, material_error> acquire(
            const MaterialConfig& config);
        void release(strview name);
        [[nodiscard]] result<void, material_error> set_sampler(
            Material& material, TextureUse use, const SamplerConfig& sampling);

        [[nodiscard]] result<void, material_error> set_diffuse_texture(
            Material& material,
            strview texture_name);
        [[nodiscard]] result<void, material_error> set_texture_maps(
            Material& material,
            strview diffuse_texture_name,
            strview specular_texture_name,
            strview normal_texture_name);
        [[nodiscard]] result<void, material_error> set_specular_texture(
            Material& material,
            strview texture_name);
        [[nodiscard]] result<void, material_error> set_normal_texture(
            Material& material,
            strview texture_name);
        [[nodiscard]] result<void, material_error> set_diffuse_color(
            Material& material,
            const glm::vec4& color);
        [[nodiscard]] result<void, material_error> set_shininess(
            Material& material,
            f32 shininess);

        [[nodiscard]] result<void, material_error> apply_global(
            MaterialType type,
            const glm::mat4& projection,
            const glm::mat4& view,
            const glm::vec3& view_position,
            const SceneLighting& lighting,
            RenderViewMode render_view_mode = RenderViewMode::default_lit);
        [[nodiscard]] result<void, material_error> apply_instance(
            Material& material,
            u64 frame_number);
        [[nodiscard]] result<void, material_error> apply_local(
            const Material& material,
            const glm::mat4& model);

        Material& default_material() noexcept { return m_default_material; }
        const Material& default_material() const noexcept {
            return m_default_material;
        }
        Material& default_ui_material() noexcept {
            return m_default_ui_material;
        }
        const Material& default_ui_material() const noexcept {
            return m_default_ui_material;
        }

        u32 reference_count(strview name) const noexcept;
        u32 loaded_count() const noexcept { return m_loaded_count; }

    private:
        struct MaterialReference {
            u64 reference_count = 0;
            u32 slot = numeric::invalid_id;
            bool auto_release = false;
        };

        struct UniformBindings {
            ShaderHandle shader{};
            ShaderUniformHandle projection{};
            ShaderUniformHandle view{};
            ShaderUniformHandle diffuse_color{};
            ShaderUniformHandle diffuse_texture{};
            ShaderUniformHandle cube_texture{};
            ShaderUniformHandle model{};
            ShaderUniformHandle ambient_color{};
            ShaderUniformHandle directional_light_direction{};
            ShaderUniformHandle directional_light_color{};
            ShaderUniformHandle normal_matrix{};
            ShaderUniformHandle specular_texture{};
            ShaderUniformHandle shininess{};
            ShaderUniformHandle blend_mode{};
            ShaderUniformHandle alpha_cutoff{};
            ShaderUniformHandle view_position{};
            ShaderUniformHandle normal_texture{};
            ShaderUniformHandle point_light_count{};
            ShaderUniformHandle point_lights{};
            ShaderUniformHandle render_view_mode{};

            [[nodiscard]] bool valid(MaterialType type) const noexcept {
                const bool transforms_valid = shader.valid() &&
                    projection.valid() && view.valid() && model.valid();
                if (type == MaterialType::skybox)
                    return transforms_valid && cube_texture.valid();
                const bool surface_valid = transforms_valid &&
                    diffuse_color.valid() && diffuse_texture.valid();
                return surface_valid &&
                    (type == MaterialType::ui ||
                     (type == MaterialType::world && ambient_color.valid() &&
                      directional_light_direction.valid() &&
                      directional_light_color.valid() &&
                      normal_matrix.valid() && specular_texture.valid() &&
                      shininess.valid() && blend_mode.valid() &&
                      alpha_cutoff.valid() && view_position.valid() &&
                      normal_texture.valid() && point_light_count.valid() &&
                      point_lights.valid() && render_view_mode.valid()));
            }
        };

        [[nodiscard]] result<void, material_error> init(
            mem::Allocator& allocator,
            ShaderSystem& shaders,
            TextureSystem& textures,
            ResourceSystem& resources,
            u32 max_material_count);
        void shutdown();
        [[nodiscard]] result<void, material_error> create_default_materials();
        [[nodiscard]] result<UniformBindings, material_error>
        resolve_bindings(strview shader_name, MaterialType type);
        [[nodiscard]] const UniformBindings* bindings(
            MaterialType type) const noexcept;
        [[nodiscard]] result<void, material_error> load_material(
            const MaterialConfig& config,
            Material& material);
        void destroy_material(Material& material);
        [[nodiscard]] result<void, material_error> acquire_samplers(Material& material);
        void release_samplers(Material& material);
        u32 find_free_slot() const noexcept;

        mem::Allocator* m_allocator = nullptr;
        ShaderSystem* m_shaders = nullptr;
        TextureSystem* m_textures = nullptr;
        ResourceSystem* m_resources = nullptr;
        cl::arr<Material> m_materials;
        cl::map<str, MaterialReference> m_references;
        Material m_default_material{};
        Material m_default_ui_material{};
        UniformBindings m_world_bindings{};
        UniformBindings m_ui_bindings{};
        UniformBindings m_skybox_bindings{};
        u32 m_loaded_count = 0;
        bool m_initialized = false;
    };
}
