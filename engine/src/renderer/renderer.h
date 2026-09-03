#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/vertex_2d.h>

#include "resources/texture.h"
#include "renderer/geometry_render_data.h"
#include "renderer/renderer_result.h"
#include "renderer/shader.h"
#include "core/result.h"
#include "core/str.h"

namespace nk {
    namespace mem { class Allocator; }
    class Platform;
    class ResourceSystem;

    struct RenderPacket {
        f64 delta_time;
        u32 geometry_count = 0;
        const GeometryRenderData* geometries = nullptr;
        u32 ui_geometry_count = 0;
        const GeometryRenderData* ui_geometries = nullptr;
    };

    enum class RenderPassKind : u8 {
        world,
        ui,
    };

    class Renderer {
    public:
        virtual ~Renderer() = default;

        [[nodiscard]] static result<Renderer*, renderer_error> create(
            mem::Allocator* allocator,
            Platform* platform,
            ResourceSystem* resources,
            strview application_name);
        static void destroy(mem::Allocator* allocator, Renderer* renderer);

        [[nodiscard]] virtual result<frame_outcome, renderer_error> draw_frame(
            const RenderPacket& packet);

        void resize(u32 width, u32 height);

        [[nodiscard]] virtual result<ShaderHandle, renderer_error> create_shader(
            const ShaderConfig& config,
            RenderPassKind render_pass) = 0;
        [[nodiscard]] virtual result<void, renderer_error> destroy_shader(
            ShaderHandle shader) = 0;
        [[nodiscard]] virtual result<void, renderer_error> use_shader(
            ShaderHandle shader) = 0;
        [[nodiscard]] virtual result<void, renderer_error> bind_shader_globals(
            ShaderHandle shader) = 0;
        [[nodiscard]] virtual result<void, renderer_error> bind_shader_instance(
            ShaderHandle shader,
            u32 instance_id) = 0;
        [[nodiscard]] virtual result<void, renderer_error> apply_shader_globals(
            ShaderHandle shader) = 0;
        [[nodiscard]] virtual result<void, renderer_error> apply_shader_instance(
            ShaderHandle shader) = 0;
        [[nodiscard]] virtual result<u32, renderer_error>
        acquire_shader_instance(ShaderHandle shader) = 0;
        [[nodiscard]] virtual result<void, renderer_error>
        release_shader_instance(ShaderHandle shader, u32 instance_id) = 0;
        [[nodiscard]] virtual result<void, renderer_error> set_shader_sampler(
            ShaderHandle shader,
            ShaderUniformHandle uniform,
            Texture* texture) = 0;

        template <ShaderUniformValue T>
        [[nodiscard]] result<void, renderer_error> set_shader_uniform(
            const ShaderHandle shader,
            const ShaderUniformHandle uniform,
            const T& value) {
            return set_shader_uniform_raw(
                shader,
                uniform,
                shader_uniform_type<T>(),
                &value,
                sizeof(T));
        }

        [[nodiscard]] result<void, renderer_error> set_shader_uniform_custom(
            const ShaderHandle shader,
            const ShaderUniformHandle uniform,
            const void* data,
            const u32 size) {
            return set_shader_uniform_raw(
                shader,
                uniform,
                ShaderUniformType::custom,
                data,
                size);
        }

        void set_default_texture(Texture* texture) {
            m_default_texture = texture;
            on_default_texture_changed(texture);
        }
        [[nodiscard]] virtual result<void, renderer_error> create_texture(
            strview name,
            u32 width,
            u32 height,
            u32 channel_count,
            const u8* pixels,
            bool has_transparency,
            Texture* out_texture) = 0;
        virtual void destroy_texture(Texture* texture) = 0;
        [[nodiscard]] virtual result<void, renderer_error> create_material(
            Material& material) = 0;
        virtual void destroy_material(Material& material) = 0;
        [[nodiscard]] virtual result<void, renderer_error> create_geometry(
            Geometry& geometry,
            cl::slice<const glm::Vertex3D> vertices,
            cl::slice<const u32> indices) = 0;
        [[nodiscard]] virtual result<void, renderer_error> create_geometry(
            Geometry& geometry,
            cl::slice<const glm::Vertex2D> vertices,
            cl::slice<const u32> indices) = 0;
        virtual void destroy_geometry(Geometry& geometry) = 0;

        void set_view(glm::mat4 view) { m_view = view; }

    protected:
        Renderer(
            mem::Allocator& allocator,
            ResourceSystem* resources,
            strview application_name)
            : m_application_name{allocator, application_name},
              m_resources{resources} {}

        [[nodiscard]] virtual result<void, renderer_error> init() = 0;
        virtual void shutdown() = 0;
        virtual void on_resized(u32 width, u32 height) = 0;
        [[nodiscard]] virtual result<frame_outcome, renderer_error> begin_frame(
            f64 delta_time) = 0;
        virtual void begin_render_pass(RenderPassKind pass) = 0;
        virtual void end_render_pass(RenderPassKind pass) = 0;
        [[nodiscard]] virtual result<void, renderer_error>
        set_shader_uniform_raw(
            ShaderHandle shader,
            ShaderUniformHandle uniform,
            ShaderUniformType type,
            const void* data,
            u32 size) = 0;
        virtual void draw_geometry(
            RenderPassKind pass,
            GeometryRenderData data) = 0;
        virtual void on_default_texture_changed(Texture*) {}
        [[nodiscard]] virtual result<frame_outcome, renderer_error> end_frame(
            f64 delta_time) = 0;

        str m_application_name;
        Platform* m_platform = nullptr;

        mem::Allocator* m_allocator = nullptr;
        ResourceSystem* m_resources = nullptr;

        u64 m_frame_number = 0;

        glm::mat4 m_projection;
        glm::mat4 m_view;
        glm::mat4 m_ui_projection;
        glm::mat4 m_ui_view{1.0f};
        f32 m_near_clip = 0.0f;
        f32 m_far_clip = 0.0f;

        Texture* m_default_texture = nullptr;
        ShaderHandle m_world_shader;
        ShaderHandle m_ui_shader;
    private:
        [[nodiscard]] result<void, renderer_error> draw_render_pass(
            RenderPassKind pass,
            ShaderHandle shader,
            const glm::mat4& projection,
            const glm::mat4& view,
            u32 geometry_count,
            const GeometryRenderData* geometries);
        [[nodiscard]] result<frame_outcome, renderer_error> end_frame_impl(
            f64 delta_time);

    };
}
