#include "geometry_render_data.h"
#include "nkpch.h"
#include "core/input.h"
#include "core/format.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
#include "resources/image_loader.h"
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
        if (renderer->m_diffuse_texture.m_internal_data != nullptr)
            renderer->destroy_texture(&renderer->m_diffuse_texture);
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
        data.textures[0] = &m_diffuse_texture;
        update_object(data);

        return end_frame_impl(packet.delta_time);
    }

    void Renderer::resize(u32 width, u32 height) {
        m_projection = glm::perspective(
            glm::radians(45.0f), width / static_cast<f32>(height), m_near_clip, m_far_clip);
        on_resized(width, height);
    }

    result<void, renderer_error> Renderer::load_texture(
        const strview name,
        Texture& texture) {
        if (m_allocator == nullptr || name.empty())
            std::abort();

        strbuf<256> path;
        if (!format_to(path, "assets/textures/{}.png", name)) {
            return err(renderer_error{
                .code = renderer_error_code::texture_path_failed,
                .native_code = 0,
            });
        }

        auto image = ImageLoader::load_png(*m_allocator, path.view());
        if (!image) {
            const image_error error = image.error();
            renderer_error_code code = renderer_error_code::texture_decode_failed;
            i32 native_code = error.native_code;
            switch (error.code) {
                case image_error_code::file_failed:
                    code = renderer_error_code::texture_file_failed;
                    native_code = static_cast<i32>(error.file);
                    break;
                case image_error_code::decode_failed:
                    break;
                case image_error_code::limits_exceeded:
                    code = renderer_error_code::texture_limits_exceeded;
                    break;
                case image_error_code::out_of_memory:
                    code = renderer_error_code::out_of_memory;
                    break;
            }
            return err(renderer_error{
                .code = code,
                .native_code = native_code,
            });
        }

        Texture replacement{};
        auto created = create_texture(
            name,
            false,
            image->width,
            image->height,
            image->channel_count,
            image->pixels.data(),
            image->has_transparency,
            &replacement);
        if (!created)
            return err(created.error());

        replacement.generation = texture.valid()
            ? texture.generation + 1
            : 0;
        if (replacement.generation == numeric::invalid_id)
            replacement.generation = 0;

        Texture previous = texture;
        texture = replacement;
        if (previous.m_internal_data != nullptr)
            destroy_texture(&previous);

        InfoLog(
            "Texture '{}' loaded ({}x{}, {} channels, generation {}).",
            name,
            texture.width,
            texture.height,
            texture.channel_count,
            texture.generation);
        return ok();
    }

    result<void, renderer_error> Renderer::cycle_debug_texture() {
        constexpr strview texture_names[]{
            strview{"cobblestone", 11},
            strview{"paving", 6},
            strview{"paving2", 7},
        };
        constexpr u8 texture_count =
            sizeof(texture_names) / sizeof(texture_names[0]);

        const strview name = texture_names[m_debug_texture_index];
        auto loaded = load_texture(name, m_diffuse_texture);
        if (!loaded)
            return err(loaded.error());

        m_debug_texture_index =
            static_cast<u8>((m_debug_texture_index + 1) % texture_count);
        return ok();
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
