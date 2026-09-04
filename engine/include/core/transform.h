#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>

#include "core/defines.h"
#include "core/result.h"

namespace nk {
    enum class transform_error_code : u8 {
        self_parent,
        cycle,
    };

    struct transform_error {
        transform_error_code code;
    };

    // Stores a local TRS and an intrusive, non-owning hierarchy. A Transform
    // automatically detaches its children when destroyed and repairs hierarchy
    // links when moved, so container relocation cannot leave dangling links.
    class Transform final {
    public:
        Transform() noexcept = default;
        explicit Transform(const glm::vec3& position) noexcept;
        explicit Transform(const glm::quat& rotation) noexcept;
        Transform(
            const glm::vec3& position,
            const glm::quat& rotation) noexcept;
        Transform(
            const glm::vec3& position,
            const glm::quat& rotation,
            const glm::vec3& scale) noexcept;
        ~Transform() noexcept;

        Transform(const Transform&) = delete;
        Transform& operator=(const Transform&) = delete;
        Transform(Transform&& other) noexcept;
        Transform& operator=(Transform&& other) noexcept;

        [[nodiscard]] Transform* parent() noexcept { return m_parent; }
        [[nodiscard]] const Transform* parent() const noexcept {
            return m_parent;
        }
        [[nodiscard]] Transform* first_child() noexcept {
            return m_first_child;
        }
        [[nodiscard]] const Transform* first_child() const noexcept {
            return m_first_child;
        }
        [[nodiscard]] Transform* next_sibling() noexcept {
            return m_next_sibling;
        }
        [[nodiscard]] const Transform* next_sibling() const noexcept {
            return m_next_sibling;
        }
        [[nodiscard]] u32 child_count() const noexcept;

        [[nodiscard]] result<void, transform_error> set_parent(
            Transform* parent) noexcept;
        void clear_parent() noexcept;

        [[nodiscard]] const glm::vec3& position() const noexcept {
            return m_position;
        }
        [[nodiscard]] const glm::quat& rotation() const noexcept {
            return m_rotation;
        }
        [[nodiscard]] const glm::vec3& scale() const noexcept {
            return m_scale;
        }

        void set_position(const glm::vec3& position) noexcept;
        void translate(const glm::vec3& translation) noexcept;
        void set_rotation(const glm::quat& rotation) noexcept;
        void rotate(const glm::quat& rotation) noexcept;
        void set_scale(const glm::vec3& scale) noexcept;
        void scale_by(const glm::vec3& scale) noexcept;
        void set_position_rotation(
            const glm::vec3& position,
            const glm::quat& rotation) noexcept;
        void set_position_rotation_scale(
            const glm::vec3& position,
            const glm::quat& rotation,
            const glm::vec3& scale) noexcept;
        void translate_rotate(
            const glm::vec3& translation,
            const glm::quat& rotation) noexcept;

        [[nodiscard]] const glm::mat4& local_matrix() const noexcept;
        [[nodiscard]] const glm::mat4& world_matrix() const noexcept;
        [[nodiscard]] bool local_dirty() const noexcept {
            return m_local_dirty;
        }
        [[nodiscard]] bool world_dirty() const noexcept {
            return m_world_dirty;
        }

    private:
        static glm::quat normalized(const glm::quat& rotation) noexcept;

        void mark_local_dirty() noexcept;
        void mark_world_dirty() noexcept;
        void unlink_from_parent() noexcept;
        void link_to_parent(Transform& parent) noexcept;
        void detach_children() noexcept;
        void disconnect() noexcept;
        void move_from(Transform& other) noexcept;

        glm::vec3 m_position{0.0f};
        glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 m_scale{1.0f};
        mutable glm::mat4 m_local_matrix{1.0f};
        mutable glm::mat4 m_world_matrix{1.0f};
        mutable bool m_local_dirty = false;
        mutable bool m_world_dirty = false;

        Transform* m_parent = nullptr;
        Transform* m_first_child = nullptr;
        Transform* m_previous_sibling = nullptr;
        Transform* m_next_sibling = nullptr;
    };
}
