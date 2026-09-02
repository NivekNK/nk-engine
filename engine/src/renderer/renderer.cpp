#include "geometry_render_data.h"
#include "nkpch.h"
#include "core/input.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
#include "vulkan/vulkan_renderer.h"
#include "platform/platform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace nk {
    result<Renderer*, renderer_error> Renderer::create(
        mem::Allocator* allocator,
        Platform* platform,
        const strview application_name) {
        if (allocator == nullptr || platform == nullptr)
            std::abort();

        auto renderer = allocator->construct_t(
            VulkanRenderer,
            *allocator,
            application_name);
        if (renderer == nullptr)
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });

        renderer->m_platform = platform;

        renderer->m_allocator = native_construct(mem::MallocAllocator);
        if (renderer->m_allocator == nullptr) {
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(renderer_error{
                .code = renderer_error_code::out_of_memory,
                .native_code = 0,
            });
        }
        if (renderer->m_allocator->allocator_init(
                mem::MallocAllocator,
                "Renderer",
                MemoryType::Renderer) == nullptr) {
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(renderer_error{
                .code = renderer_error_code::initialization_failed,
                .native_code = 0,
            });
        }

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
            
        auto initialized = renderer->init();
        if (!initialized) {
            const renderer_error error = initialized.error();
            renderer->shutdown();
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(error);
        }

        auto texture_created = renderer->create_texture(
            "default",
            false,
            tex_dimension,
            tex_dimension,
            channels,
            pixels,
            false,
            &renderer->m_default_texture);
        if (!texture_created) {
            const renderer_error error = texture_created.error();
            renderer->shutdown();
            native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
            allocator->deconstruct_t(VulkanRenderer, renderer);
            return err(error);
        }

        return ok(static_cast<Renderer*>(renderer));
    }

    void Renderer::destroy(mem::Allocator* allocator, Renderer* renderer) {
        if (allocator == nullptr || renderer == nullptr)
            return;
        if (renderer->m_default_texture.m_internal_data != nullptr)
            renderer->destroy_texture(&renderer->m_default_texture);
        renderer->shutdown();
        native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
        allocator->deconstruct_t(VulkanRenderer, renderer);
    }

    result<frame_outcome, renderer_error> Renderer::draw_frame(
        const RenderPacket& packet) {
        auto begun = begin_frame(packet.delta_time);
        if (!begun)
            return err(begun.error());
        if (*begun == frame_outcome::skipped_swapchain_recreation)
            return ok(frame_outcome::skipped_swapchain_recreation);

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

        return end_frame_impl(packet.delta_time);
    }

    void Renderer::resize(u32 width, u32 height) {
        m_projection = glm::perspective(
            glm::radians(45.0f), width / static_cast<f32>(height), m_near_clip, m_far_clip);
        on_resized(width, height);
    }

    result<frame_outcome, renderer_error> Renderer::end_frame_impl(
        const f64 delta_time) {
        auto ended = end_frame(delta_time);
        if (!ended)
            return err(ended.error());
        if (*ended == frame_outcome::rendered)
            ++m_frame_number;
        return ended;
    }
}
