#include <core/app_creator.h>

#include <core/camera.h>
#include <core/input.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/epsilon.hpp>

class Editor : public nk::App {
public:
    Editor() : nk::App({
        .name = "Editor",
        .start_pos_x = 100,
        .start_pos_y = 100,
        .start_width = 1280,
        .start_height = 720
    }) {
        m_camera_position = glm::vec3(0.0f, 0.0f, 30.0f);
        m_camera_euler = glm::vec3(0.0f, 0.0f, 0.0f);
        m_transform = glm::translate(glm::mat4(1.0f), m_camera_position);
        m_view = glm::inverse(m_transform);
        m_camera_dirty = true;
        DebugLog("Editor created.");
    }

    virtual ~Editor() override {
        DebugLog("Editor destroyed.");
    }

    virtual bool update(nk::f64 delta_time) override {
        if (nk::Input::is_key_down(nk::KeyCode::A) || nk::Input::is_key_down(nk::KeyCode::Left)) {
            camera_yaw(1.0f * delta_time);
        }
        if (nk::Input::is_key_down(nk::KeyCode::D) || nk::Input::is_key_down(nk::KeyCode::Right)) {
            camera_yaw(-1.0f * delta_time);
        }
        if (nk::Input::is_key_down(nk::KeyCode::Up)) {
            camera_pitch(1.0f * delta_time);
        }
        if (nk::Input::is_key_down(nk::KeyCode::Down)) {
            camera_pitch(-1.0f * delta_time);
        }

        nk::f32 temp_move_speed = 100.0f;
        glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);

        if (nk::Input::is_key_down(nk::KeyCode::W)) {
            glm::vec3 forward = get_forward(m_transform);
            velocity += forward;
        }
        if (nk::Input::is_key_down(nk::KeyCode::S)) {
            glm::vec3 backward = get_backward(m_transform);
            velocity += backward;
        }
        if (nk::Input::is_key_down(nk::KeyCode::Q)) {
            glm::vec3 left = get_left(m_transform);
            velocity += left;
        }
        if (nk::Input::is_key_down(nk::KeyCode::E)) {
            glm::vec3 right = get_right(m_transform);
            velocity += right;
        }
        
        if (nk::Input::is_key_down(nk::KeyCode::Space)) {
            velocity.y += 1.0f;
        }
        if (nk::Input::is_key_down(nk::KeyCode::X)) {
            velocity.y -= 1.0f;
        }

        glm::vec3 z = glm::vec3(0.0f, 0.0f, 0.0f);
        if (!glm::all(glm::epsilonEqual(z, velocity, 0.0002f))) {
            velocity = glm::normalize(velocity);
            m_camera_position.x += velocity.x * temp_move_speed * delta_time;
            m_camera_position.y += velocity.y * temp_move_speed * delta_time;
            m_camera_position.z += velocity.z * temp_move_speed * delta_time;
            m_camera_dirty = true;
        }

        update_camera_transform();

        nk::Camera::set_view(m_view);

        return true;
    }

private:
    void update_camera_transform() {
        if (!m_camera_dirty)
            return;

        glm::mat4 rotation = glm::eulerAngleXYZ(m_camera_euler.x, m_camera_euler.y, m_camera_euler.z);
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), m_camera_position);
        m_transform = translation * rotation;
        m_view = glm::inverse(m_transform);
        m_camera_dirty = false;
    }

    void camera_yaw(nk::f32 amount) {
        m_camera_euler.y += amount;
        m_camera_dirty = true;
    }

    void camera_pitch(nk::f32 amount) {
        m_camera_euler.x += amount;

        nk::f32 limit = glm::radians(89.0f);
        m_camera_euler.x = glm::clamp(m_camera_euler.x, -limit, limit);

        m_camera_dirty = true;
    }

    glm::vec3 get_forward(const glm::mat4 &transform) {
        return glm::normalize(-glm::vec3(transform[2])); // -Z
    }
    
    glm::vec3 get_backward(const glm::mat4 &transform) {
        return -get_forward(transform);
    }
    
    glm::vec3 get_right(const glm::mat4 &transform) {
        return glm::normalize(glm::vec3(transform[0]));  // +X
    }

    glm::vec3 get_left(const glm::mat4 &transform) {
        return -get_right(transform);
    }

    glm::mat4 m_view;
    glm::mat4 m_transform;
    glm::vec3 m_camera_position;
    glm::vec3 m_camera_euler;
    bool m_camera_dirty;
};

CREATE_APP(Editor)
