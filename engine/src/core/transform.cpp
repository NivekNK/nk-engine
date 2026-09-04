#include "nkpch.h"

#include "core/transform.h"

#include <cmath>
#include <utility>

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace nk {
    Transform::Transform(const glm::vec3& position) noexcept
        : m_position{position},
          m_local_dirty{true},
          m_world_dirty{true} {}

    Transform::Transform(const glm::quat& rotation) noexcept
        : m_rotation{normalized(rotation)},
          m_local_dirty{true},
          m_world_dirty{true} {}

    Transform::Transform(
        const glm::vec3& position,
        const glm::quat& rotation) noexcept
        : m_position{position},
          m_rotation{normalized(rotation)},
          m_local_dirty{true},
          m_world_dirty{true} {}

    Transform::Transform(
        const glm::vec3& position,
        const glm::quat& rotation,
        const glm::vec3& scale) noexcept
        : m_position{position},
          m_rotation{normalized(rotation)},
          m_scale{scale},
          m_local_dirty{true},
          m_world_dirty{true} {}

    Transform::~Transform() noexcept {
        disconnect();
    }

    Transform::Transform(Transform&& other) noexcept {
        move_from(other);
    }

    Transform& Transform::operator=(Transform&& other) noexcept {
        if (this == &other)
            return *this;

        disconnect();
        move_from(other);
        return *this;
    }

    u32 Transform::child_count() const noexcept {
        u32 count = 0;
        for (const Transform* child = m_first_child;
             child != nullptr;
             child = child->m_next_sibling) {
            ++count;
        }
        return count;
    }

    result<void, transform_error> Transform::set_parent(
        Transform* parent) noexcept {
        if (parent == this) {
            return err(transform_error{
                transform_error_code::self_parent,
            });
        }

        for (Transform* ancestor = parent;
             ancestor != nullptr;
             ancestor = ancestor->m_parent) {
            if (ancestor == this) {
                return err(transform_error{
                    transform_error_code::cycle,
                });
            }
        }

        if (m_parent == parent)
            return ok();

        unlink_from_parent();
        if (parent != nullptr)
            link_to_parent(*parent);
        mark_world_dirty();
        return ok();
    }

    void Transform::clear_parent() noexcept {
        if (m_parent == nullptr)
            return;
        unlink_from_parent();
        mark_world_dirty();
    }

    void Transform::set_position(const glm::vec3& position) noexcept {
        m_position = position;
        mark_local_dirty();
    }

    void Transform::translate(const glm::vec3& translation) noexcept {
        m_position += translation;
        mark_local_dirty();
    }

    void Transform::set_rotation(const glm::quat& rotation) noexcept {
        m_rotation = normalized(rotation);
        mark_local_dirty();
    }

    void Transform::rotate(const glm::quat& rotation) noexcept {
        m_rotation = normalized(m_rotation * normalized(rotation));
        mark_local_dirty();
    }

    void Transform::set_scale(const glm::vec3& scale) noexcept {
        m_scale = scale;
        mark_local_dirty();
    }

    void Transform::scale_by(const glm::vec3& scale) noexcept {
        m_scale *= scale;
        mark_local_dirty();
    }

    void Transform::set_position_rotation(
        const glm::vec3& position,
        const glm::quat& rotation) noexcept {
        m_position = position;
        m_rotation = normalized(rotation);
        mark_local_dirty();
    }

    void Transform::set_position_rotation_scale(
        const glm::vec3& position,
        const glm::quat& rotation,
        const glm::vec3& scale) noexcept {
        m_position = position;
        m_rotation = normalized(rotation);
        m_scale = scale;
        mark_local_dirty();
    }

    void Transform::translate_rotate(
        const glm::vec3& translation,
        const glm::quat& rotation) noexcept {
        m_position += translation;
        m_rotation = normalized(m_rotation * normalized(rotation));
        mark_local_dirty();
    }

    const glm::mat4& Transform::local_matrix() const noexcept {
        if (m_local_dirty) {
            m_local_matrix =
                glm::translate(glm::mat4{1.0f}, m_position) *
                glm::mat4_cast(m_rotation) *
                glm::scale(glm::mat4{1.0f}, m_scale);
            m_local_dirty = false;
        }
        return m_local_matrix;
    }

    const glm::mat4& Transform::world_matrix() const noexcept {
        if (m_world_dirty) {
            const glm::mat4& local = local_matrix();
            m_world_matrix = m_parent == nullptr
                ? local
                : m_parent->world_matrix() * local;
            m_world_dirty = false;
        }
        return m_world_matrix;
    }

    glm::quat Transform::normalized(const glm::quat& rotation) noexcept {
        const f32 length_squared = glm::dot(rotation, rotation);
        if (!std::isfinite(length_squared) || length_squared <= 1.0e-20f)
            return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
        return rotation * (1.0f / std::sqrt(length_squared));
    }

    void Transform::mark_local_dirty() noexcept {
        m_local_dirty = true;
        mark_world_dirty();
    }

    void Transform::mark_world_dirty() noexcept {
        m_world_dirty = true;
        for (Transform* child = m_first_child;
             child != nullptr;
             child = child->m_next_sibling) {
            child->mark_world_dirty();
        }
    }

    void Transform::unlink_from_parent() noexcept {
        if (m_parent != nullptr && m_parent->m_first_child == this)
            m_parent->m_first_child = m_next_sibling;
        if (m_previous_sibling != nullptr)
            m_previous_sibling->m_next_sibling = m_next_sibling;
        if (m_next_sibling != nullptr)
            m_next_sibling->m_previous_sibling = m_previous_sibling;

        m_parent = nullptr;
        m_previous_sibling = nullptr;
        m_next_sibling = nullptr;
    }

    void Transform::link_to_parent(Transform& parent) noexcept {
        m_parent = &parent;
        m_next_sibling = parent.m_first_child;
        if (m_next_sibling != nullptr)
            m_next_sibling->m_previous_sibling = this;
        parent.m_first_child = this;
    }

    void Transform::detach_children() noexcept {
        Transform* child = m_first_child;
        m_first_child = nullptr;
        while (child != nullptr) {
            Transform* next = child->m_next_sibling;
            child->m_parent = nullptr;
            child->m_previous_sibling = nullptr;
            child->m_next_sibling = nullptr;
            child->mark_world_dirty();
            child = next;
        }
    }

    void Transform::disconnect() noexcept {
        detach_children();
        unlink_from_parent();
    }

    void Transform::move_from(Transform& other) noexcept {
        m_position = other.m_position;
        m_rotation = other.m_rotation;
        m_scale = other.m_scale;
        m_local_matrix = other.m_local_matrix;
        m_world_matrix = other.m_world_matrix;
        m_local_dirty = other.m_local_dirty;
        m_world_dirty = other.m_world_dirty;

        m_parent = other.m_parent;
        m_first_child = other.m_first_child;
        m_previous_sibling = other.m_previous_sibling;
        m_next_sibling = other.m_next_sibling;

        if (m_parent != nullptr && m_parent->m_first_child == &other)
            m_parent->m_first_child = this;
        if (m_previous_sibling != nullptr)
            m_previous_sibling->m_next_sibling = this;
        if (m_next_sibling != nullptr)
            m_next_sibling->m_previous_sibling = this;
        for (Transform* child = m_first_child;
             child != nullptr;
             child = child->m_next_sibling) {
            child->m_parent = this;
        }

        other.m_position = glm::vec3{0.0f};
        other.m_rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
        other.m_scale = glm::vec3{1.0f};
        other.m_local_matrix = glm::mat4{1.0f};
        other.m_world_matrix = glm::mat4{1.0f};
        other.m_local_dirty = false;
        other.m_world_dirty = false;
        other.m_parent = nullptr;
        other.m_first_child = nullptr;
        other.m_previous_sibling = nullptr;
        other.m_next_sibling = nullptr;
    }
}
