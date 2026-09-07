#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/vertex_2d.h>

#include "resources/texture.h"
#include "renderer/geometry_render_data.h"
#include "renderer/lighting.h"
#include "renderer/renderer_result.h"
#include "renderer/shader.h"
#include "core/result.h"
#include "core/str.h"

namespace nk {
    namespace mem { class Allocator; }
    class Platform;
    class ResourceSystem;
    class MaterialSystem;
    class Mesh;
    enum class SystemEventCode : u16;
    struct EventContext;

    bool on_render_view_mode(SystemEventCode, void*, void*, EventContext);

    struct RenderPacket {
        f64 delta_time;
        SceneLighting lighting{};
        u32 geometry_count = 0;
        const GeometryRenderData* geometries = nullptr;
        u32 mesh_count = 0;
        const Mesh* meshes = nullptr;
        u32 ui_geometry_count = 0;
        const GeometryRenderData* ui_geometries = nullptr;
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
            MaterialSystem& materials,
            const RenderPacket& packet);

        void resize(u32 width, u32 height);
        // Most recently retired GPU sample; negative when profiling is disabled
        // or this frame has no completed timestamp pair. Never waits for the GPU.
        [[nodiscard]] virtual f64 gpu_frame_ms() const noexcept { return -1.0; }

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
            ShaderHandle shader,
            bool needs_update) = 0;
        [[nodiscard]] virtual result<u32, renderer_error>
        acquire_shader_instance(ShaderHandle shader) = 0;
        [[nodiscard]] virtual result<void, renderer_error>
        release_shader_instance(ShaderHandle shader, u32 instance_id) = 0;
        [[nodiscard]] virtual result<void, renderer_error> set_shader_sampler(
            ShaderHandle shader,
            ShaderUniformHandle uniform,
            Texture* texture,
            u32 array_index = 0) = 0;

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
        [[nodiscard]] virtual result<void, renderer_error> create_geometry(
            Geometry& geometry,
            cl::slice<const glm::Vertex3D> vertices,
            cl::slice<const u32> indices) = 0;
        [[nodiscard]] virtual result<void, renderer_error> create_geometry(
            Geometry& geometry,
            cl::slice<const glm::Vertex2D> vertices,
            cl::slice<const u32> indices) = 0;
        virtual void destroy_geometry(Geometry& geometry) = 0;

        void set_view(glm::mat4 view, glm::vec3 position) {
            m_view = view;
            m_view_position = position;
        }
        [[nodiscard]] bool set_render_view_mode(
            const RenderViewMode mode) noexcept {
            if (!valid_render_view_mode(mode))
                return false;
            m_render_view_mode = mode;
            return true;
        }
        [[nodiscard]] RenderViewMode render_view_mode() const noexcept {
            return m_render_view_mode;
        }

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
        glm::vec3 m_view_position{};
        glm::mat4 m_ui_projection;
        glm::mat4 m_ui_view{1.0f};
        f32 m_near_clip = 0.0f;
        f32 m_far_clip = 0.0f;

        Texture* m_default_texture = nullptr;
        RenderViewMode m_render_view_mode = RenderViewMode::default_lit;
    private:
        [[nodiscard]] result<void, renderer_error> draw_render_pass(
            MaterialSystem& materials,
            RenderPassKind pass,
            const glm::mat4& projection,
            const glm::mat4& view,
            const glm::vec3& view_position,
            const SceneLighting& lighting,
            u32 geometry_count,
            const GeometryRenderData* geometries,
            u32 mesh_count,
            const Mesh* meshes);
        [[nodiscard]] result<void, renderer_error> draw_render_data(
            MaterialSystem& materials,
            RenderPassKind pass,
            MaterialType expected_material_type,
            GeometryRenderData data,
            Material*& bound_material);
        [[nodiscard]] result<frame_outcome, renderer_error> end_frame_impl(
            f64 delta_time);

    };
}
