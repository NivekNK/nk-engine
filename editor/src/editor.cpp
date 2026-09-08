#include <core/app_creator.h>

#include <core/camera.h>
#include <core/input.h>

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
        DebugLog("Editor created.");
    }

    virtual ~Editor() override {
        DebugLog("Editor destroyed.");
    }

    virtual bool update(nk::f64 delta_time) override {
        nk::Camera* camera = nk::Camera::active();
        if (camera == nullptr)
            return false;
        if (!m_camera_initialized) {
            camera->set_position({0.0f, 0.0f, 30.0f});
            m_camera_initialized = true;
        }

        if (nk::Input::is_key_down(nk::KeyCode::A) || nk::Input::is_key_down(nk::KeyCode::Left)) {
            camera->yaw(1.0f * delta_time);
        }
        if (nk::Input::is_key_down(nk::KeyCode::D) || nk::Input::is_key_down(nk::KeyCode::Right)) {
            camera->yaw(-1.0f * delta_time);
        }
        if (nk::Input::is_key_down(nk::KeyCode::Up)) {
            camera->pitch(1.0f * delta_time);
        }
        if (nk::Input::is_key_down(nk::KeyCode::Down)) {
            camera->pitch(-1.0f * delta_time);
        }

        nk::f32 temp_move_speed = 100.0f;
        glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);

        if (nk::Input::is_key_down(nk::KeyCode::W)) {
            velocity += camera->forward();
        }
        if (nk::Input::is_key_down(nk::KeyCode::S)) {
            velocity += camera->backward();
        }
        if (nk::Input::is_key_down(nk::KeyCode::Q)) {
            velocity += camera->left();
        }
        if (nk::Input::is_key_down(nk::KeyCode::E)) {
            velocity += camera->right();
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
            camera->translate(velocity * temp_move_speed *
                static_cast<nk::f32>(delta_time));
        }

        return true;
    }

private:
    bool m_camera_initialized = false;
};

CREATE_APP(Editor)
