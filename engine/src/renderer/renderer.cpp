#include "nkpch.h"
#include "core/input.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
// #include "vulkan/vulkan_renderer.h"
#include "simple-vulkan/simple_vulkan_renderer.h"
#include "platform/platform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace nk {
    Renderer* Renderer::create(mem::Allocator* allocator, Platform* platform, str application_name) {
        auto renderer = allocator->construct_t(SimpleVulkanRenderer);

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

        renderer->init();
        return static_cast<Renderer*>(renderer);
    }

    void Renderer::destroy(mem::Allocator* allocator, Renderer* renderer) {
        renderer->shutdown();
        native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
        allocator->deconstruct_t(SimpleVulkanRenderer, renderer);
    }

    bool Renderer::draw_frame(const RenderPacket& packet) {
        if (begin_frame(packet.delta_time)) {
            update_global_state(
                m_projection,
                m_view,
                glm::vec3(0.0f),
                glm::vec4(1.0f),
                0
            );

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
            update_object(model);
            
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
