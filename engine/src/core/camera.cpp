#include "nkpch.h"

#include "core/camera.h"

#include "systems/camera_system.h"

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace nk {
    namespace {
        constexpr f32 maximum_pitch =
            1.5533430342749532f; // radians(89 degrees)
    }

    Camera* Camera::active() noexcept {
        CameraSystem* system = CameraSystem::current();
        return system == nullptr ? nullptr : system->active();
    }

    void Camera::reset() noexcept {
        m_position = glm::vec3{0.0f};
        m_euler_rotation = glm::vec3{0.0f};
        m_view = glm::mat4{1.0f};
        m_dirty = false;
    }

    const glm::mat4& Camera::view() const noexcept {
        if (!m_dirty)
            return m_view;

        const glm::mat4 transform =
            glm::translate(glm::mat4{1.0f}, m_position) *
            rotation_matrix();
        m_view = glm::inverse(transform);
        m_dirty = false;
        return m_view;
    }

    void Camera::set_position(const glm::vec3& position) noexcept {
        if (m_position == position)
            return;
        m_position = position;
        m_dirty = true;
    }

    void Camera::translate(const glm::vec3& translation) noexcept {
        if (translation == glm::vec3{0.0f})
            return;
        m_position += translation;
        m_dirty = true;
    }

    void Camera::set_euler_rotation(const glm::vec3& rotation) noexcept {
        m_euler_rotation = rotation;
        clamp_pitch();
        m_dirty = true;
    }

    void Camera::set_position_rotation(
        const glm::vec3& position,
        const glm::vec3& rotation) noexcept {
        m_position = position;
        m_euler_rotation = rotation;
        clamp_pitch();
        m_dirty = true;
    }

    void Camera::yaw(const f32 radians) noexcept {
        if (radians == 0.0f)
            return;
        m_euler_rotation.y += radians;
        m_dirty = true;
    }

    void Camera::pitch(const f32 radians) noexcept {
        if (radians == 0.0f)
            return;
        m_euler_rotation.x += radians;
        clamp_pitch();
        m_dirty = true;
    }

    void Camera::roll(const f32 radians) noexcept {
        if (radians == 0.0f)
            return;
        m_euler_rotation.z += radians;
        m_dirty = true;
    }

    glm::vec3 Camera::forward() const noexcept {
        return glm::normalize(-glm::vec3{rotation_matrix()[2]});
    }

    glm::vec3 Camera::right() const noexcept {
        return glm::normalize(glm::vec3{rotation_matrix()[0]});
    }

    glm::vec3 Camera::up() const noexcept {
        return glm::normalize(glm::vec3{rotation_matrix()[1]});
    }

    glm::mat4 Camera::rotation_matrix() const noexcept {
        glm::mat4 rotation = glm::rotate(
            glm::mat4{1.0f},
            m_euler_rotation.x,
            glm::vec3{1.0f, 0.0f, 0.0f});
        rotation = glm::rotate(
            rotation,
            m_euler_rotation.y,
            glm::vec3{0.0f, 1.0f, 0.0f});
        return glm::rotate(
            rotation,
            m_euler_rotation.z,
            glm::vec3{0.0f, 0.0f, 1.0f});
    }

    void Camera::clamp_pitch() noexcept {
        m_euler_rotation.x = glm::clamp(
            m_euler_rotation.x,
            -maximum_pitch,
            maximum_pitch);
    }
}
