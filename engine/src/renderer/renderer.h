#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>

#include "resources/texture.h"
#include "renderer/geometry_render_data.h"
#include "renderer/renderer_result.h"
#include "core/result.h"
#include "core/str.h"

namespace nk {
    namespace mem { class Allocator; }
    class Platform;

    struct RenderPacket {
        f64 delta_time;
    };

    class Renderer {
    public:
        virtual ~Renderer() = default;

        [[nodiscard]] static result<Renderer*, renderer_error> create(
            mem::Allocator* allocator,
            Platform* platform,
            strview application_name);
        static void destroy(mem::Allocator* allocator, Renderer* renderer);

        [[nodiscard]] virtual result<frame_outcome, renderer_error> draw_frame(
            const RenderPacket& packet);

        void resize(u32 width, u32 height);

        [[nodiscard]] result<void, renderer_error> load_texture(
            strview name,
            Texture& texture);
        [[nodiscard]] result<void, renderer_error> cycle_debug_texture();
        
        [[nodiscard]] virtual result<void, renderer_error> create_texture(
            strview name,
            bool auto_release,
            u32 width,
            u32 height,
            u32 channel_count,
            const u8* pixels,
            bool has_transparency,
            Texture* out_texture) = 0;
        virtual void destroy_texture(Texture* texture) = 0;

        void set_view(glm::mat4 view) { m_view = view; }

    protected:
        Renderer(mem::Allocator& allocator, strview application_name)
            : m_application_name{allocator, application_name} {}

        [[nodiscard]] virtual result<void, renderer_error> init() = 0;
        virtual void shutdown() = 0;
        virtual void on_resized(u32 width, u32 height) = 0;
        [[nodiscard]] virtual result<frame_outcome, renderer_error> begin_frame(
            f64 delta_time) = 0;
        virtual void update_global_state(
            glm::mat4 projection,
            glm::mat4 view,
            glm::vec3 view_position,
            glm::vec4 ambient_color,
            i32 mode) = 0;
        virtual void update_object(GeometryRenderData data) = 0;
        [[nodiscard]] virtual result<frame_outcome, renderer_error> end_frame(
            f64 delta_time) = 0;

        str m_application_name;
        Platform* m_platform = nullptr;

        mem::Allocator* m_allocator = nullptr;

        u64 m_frame_number = 0;

        glm::mat4 m_projection;
        glm::mat4 m_view;
        f32 m_near_clip = 0.0f;
        f32 m_far_clip = 0.0f;

        Texture m_default_texture{};
        Texture m_diffuse_texture{};

    private:
        [[nodiscard]] result<frame_outcome, renderer_error> end_frame_impl(
            f64 delta_time);

        bool m_temp_active_rotation = false;
        u8 m_debug_texture_index = 0;
    };
}
