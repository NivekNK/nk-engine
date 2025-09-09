#include "nkpch.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
#include "vulkan/vulkan_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

namespace nk {
    Renderer* Renderer::create(mem::Allocator* allocator, Platform* platform, str application_name) {
        auto renderer = allocator->construct_t(VulkanRenderer);

        renderer->m_application_name = application_name;
        renderer->m_platform = platform;

        renderer->m_allocator = native_construct(mem::MallocAllocator);
        renderer->m_allocator->allocator_init(mem::MallocAllocator, "Renderer", MemoryType::Renderer);

        renderer->m_frame_number = 0;

        renderer->init();
        return renderer;
    }

    void Renderer::destroy(mem::Allocator* allocator, Renderer* renderer) {
        renderer->shutdown();
        native_deconstruct(mem::MallocAllocator, renderer->m_allocator);
        allocator->deconstruct_t(VulkanRenderer, renderer);
    }

    bool Renderer::draw_frame(const RenderPacket& packet) {
        if (begin_frame(packet.delta_time)) {
            glm::mat4 projection = glm::perspective(
                glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 1000.0f);

            static f32 z = -1.0f;
            z -= 0.01f;
            glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, z));
                
            update_global_state(
                projection,
                view,
                glm::vec3(0.0f),
                glm::vec4(1.0f),
                0
            );

            bool result = end_frame_impl(packet.delta_time);
            if (!result) {
                ErrorLog("nk::Renderer::end_frame failed. Application shutting down.");
                return false;
            }
        }
        return true;
    }

    bool Renderer::end_frame_impl(f64 delta_time) {
        bool result = end_frame(delta_time);
        m_frame_number++;
        return result;
    }
}
