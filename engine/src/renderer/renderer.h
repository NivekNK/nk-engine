#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>

namespace nk {
    namespace mem { class Allocator; }
    class Platform;

    struct RenderPacket {
        f64 delta_time;
    };

    class Renderer {
    public:
        virtual ~Renderer() = default;

        static Renderer* create(mem::Allocator* allocator, Platform* platform, str application_name);
        static void destroy(mem::Allocator* allocator, Renderer* renderer);

        bool draw_frame(const RenderPacket& packet);

        void resize(u32 width, u32 height);

        void set_view(glm::mat4 view) { m_view = view; }

    protected:
        virtual void init() = 0;
        virtual void shutdown() = 0;
        virtual void on_resized(u32 width, u32 height) = 0;
        virtual bool begin_frame(f64 delta_time) = 0;
        virtual void update_global_state(
            glm::mat4 projection,
            glm::mat4 view,
            glm::vec3 view_position,
            glm::vec4 ambient_color,
            i32 mode) = 0;
        virtual void update_object(glm::mat4 model) = 0;
        virtual bool end_frame(f64 delta_time) = 0;

        str m_application_name;
        Platform* m_platform;

        mem::Allocator* m_allocator;

        u64 m_frame_number;

        glm::mat4 m_projection;
        glm::mat4 m_view;
        f32 m_near_clip;
        f32 m_far_clip;

    private:
        bool end_frame_impl(f64 delta_time);
    };
}
