#include "nkpch.h"

#include "renderer/renderer.h"

#include "memory/malloc_allocator.h"
#include "vulkan/vulkan_renderer.h"

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
