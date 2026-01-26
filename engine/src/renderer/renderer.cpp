#include "geometry_render_data.h"
#include "nkpch.h"
#include "core/input.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
#include "vulkan/vulkan_renderer.h"
// #include "simple-vulkan/simple_vulkan_renderer.h"
#include "platform/platform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace nk {
    Renderer* Renderer::create(mem::Allocator* allocator, Platform* platform, str application_name) {
        auto renderer = allocator->construct_t(VulkanRenderer);

        renderer->m_application_name = application_name;
        renderer->m_platform = platform;

        renderer->m_allocator = native_construct(mem::MallocAllocator);
        renderer->m_allocator->allocator_init(mem::MallocAllocator, "Renderer", MemoryType::Renderer);

        renderer->m_frame_number = 0;

        renderer->m_near_clip = 0.1f;
        renderer->m_far_clip = 1000.0f;

        f32 aspect = platform->width() / static_cast<f32>(platform->height());
        renderer->m_projection = glm::perspective(
            glm::radians(45.0f), aspect, renderer->m_near_clip, renderer->m_far_clip);
        renderer->m_view = glm::inverse(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -30.0f)));

        // NOTE: Create default texture, a 256x256 blue/white checkerboard pattern
        // This is done in code to eliminate asset dependencies
        TraceLog("Creating default texture...");

        constexpr u32 tex_dimension = 256;
        constexpr u32 channels = 4;
        constexpr u32 pixel_count = tex_dimension * tex_dimension;
        u8 pixels[pixel_count * channels];
        memset(pixels, 255, sizeof(u8) * pixel_count * channels);

        for (u64 row = 0; row < tex_dimension; row++) {
            for (u64 col = 0; col < tex_dimension; col++) {
                u64 index = (row * tex_dimension) + col;
                u64 index_bpp = index * channels;
                if (row % 2) {
                    if (col % 2) {
                        pixels[index_bpp + 0] = 0;
                        pixels[index_bpp + 1] = 0;
                    }
                } else {
                    if (!(col % 2)) {
                        pixels[index_bpp + 0] = 0;
                        pixels[index_bpp + 1] = 0;
                    }
                }
            }
        }
            
        renderer->init();

        renderer->create_texture(
            "default",
            false,
            tex_dimension,
            tex_dimension,
            channels,
            pixels,
            false,
            &renderer->m_default_texture);

        return static_cast<Renderer*>(renderer);
    }

    void Renderer::destroy(mem::Allocator* allocator, Renderer* renderer) {
        renderer->destroy_texture(&renderer->m_default_texture);
        renderer->shutdown();
        native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
        allocator->deconstruct_t(VulkanRenderer, renderer);
    }

    bool Renderer::draw_frame(const RenderPacket& packet) {
        if (begin_frame(packet.delta_time)) {
            update_global_state(
                m_projection,
                m_view,
                glm::vec3(0.0f),
                glm::vec4(1.0f),
                0);

            static f32 angle = 0.01f;
            glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            if (Input::is_key_down(KeyCode::F)) {
                m_temp_active_rotation = !m_temp_active_rotation;
            }

            if (m_temp_active_rotation) {
                angle += 0.001f;
                glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
                rotation = glm::angleAxis(angle, forward);
            }

            glm::mat4 model = glm::toMat4(rotation);
            GeometryRenderData data = {};
            data.object_id = 0; // TODO: actual object_id
            data.model = model;
            data.textures[0] = &m_default_texture;
            update_object(data);

            bool result = end_frame_impl(packet.delta_time);
            if (!result) {
                ErrorLog("nk::Renderer::end_frame failed. Application shutting down.");
                return false;
            }
        }
        return true;
    }

    void Renderer::resize(u32 width, u32 height) {
        m_projection = glm::perspective(
            glm::radians(45.0f), width / static_cast<f32>(height), m_near_clip, m_far_clip);
        on_resized(width, height);
    }

    bool Renderer::end_frame_impl(f64 delta_time) {
        bool result = end_frame(delta_time);
        m_frame_number++;
        return result;
    }
}
