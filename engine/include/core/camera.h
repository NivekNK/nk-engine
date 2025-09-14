#pragma once

#include "glm/ext/matrix_float4x4.hpp"

namespace nk {
    class Renderer;

    class Camera {
    public:
        static void set_view(glm::mat4 view);

    private:
        Camera() = default;
        ~Camera() = default;

        static void init(Renderer* renderer);

        static Camera& get() {
            static Camera instance;
            return instance;
        }

        Renderer* m_renderer;

        friend class Engine;
    };
}
