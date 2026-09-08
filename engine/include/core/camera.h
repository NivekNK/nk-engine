#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

#include "core/defines.h"

namespace nk {
    class Camera final {
    public:
        Camera() noexcept = default;
        explicit Camera(const glm::vec3& position) noexcept
            : m_position{position},
              m_dirty{position != glm::vec3{0.0f}} {}

        [[nodiscard]] static Camera* active() noexcept;

        void reset() noexcept;

        [[nodiscard]] const glm::vec3& position() const noexcept {
            return m_position;
        }
        [[nodiscard]] const glm::vec3& euler_rotation() const noexcept {
            return m_euler_rotation;
        }
        [[nodiscard]] const glm::mat4& view() const noexcept;
        [[nodiscard]] bool dirty() const noexcept { return m_dirty; }

        void set_position(const glm::vec3& position) noexcept;
        void translate(const glm::vec3& translation) noexcept;
        void set_euler_rotation(const glm::vec3& rotation) noexcept;
        void set_position_rotation(
            const glm::vec3& position,
            const glm::vec3& rotation) noexcept;
        void yaw(f32 radians) noexcept;
        void pitch(f32 radians) noexcept;
        void roll(f32 radians) noexcept;

        [[nodiscard]] glm::vec3 forward() const noexcept;
        [[nodiscard]] glm::vec3 backward() const noexcept {
            return -forward();
        }
        [[nodiscard]] glm::vec3 right() const noexcept;
        [[nodiscard]] glm::vec3 left() const noexcept {
            return -right();
        }
        [[nodiscard]] glm::vec3 up() const noexcept;
        [[nodiscard]] glm::vec3 down() const noexcept {
            return -up();
        }

    private:
        [[nodiscard]] glm::mat4 rotation_matrix() const noexcept;
        void clamp_pitch() noexcept;

        glm::vec3 m_position{0.0f};
        glm::vec3 m_euler_rotation{0.0f};
        mutable glm::mat4 m_view{1.0f};
        mutable bool m_dirty = false;
    };
}
