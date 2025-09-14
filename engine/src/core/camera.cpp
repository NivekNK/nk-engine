#include "nkpch.h"

#include "core/camera.h"

#include "renderer/renderer.h"

namespace nk {
    void Camera::init(Renderer* renderer) {
        Camera& camera = get();
        camera.m_renderer = renderer;
    }

    void Camera::set_view(glm::mat4 view) {
        Camera& camera = get();
        if (!camera.m_renderer)
            return;

        camera.m_renderer->set_view(view);
    }
}
