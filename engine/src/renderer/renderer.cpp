#include "nkpch.h"

#include "renderer/renderer.h"

#include "memory/allocator.h"
#include "vulkan/renderer_backend.h"

namespace nk {
    Renderer* Renderer::create(Allocator* allocator, Window& window, cstr application_name) {
        return allocator->construct(RendererBackend, window, application_name);
    }

    void Renderer::free(Allocator* allocator, Renderer* renderer) {
        allocator->destroy(RendererBackend, renderer);
    }
}