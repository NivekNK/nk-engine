#include <gtest/gtest.h>

#include "core/camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {
    void expect_vec3_near(
        const glm::vec3& actual,
        const glm::vec3& expected,
        const float tolerance = 1.0e-5f) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }

    void expect_matrix_near(
        const glm::mat4& actual,
        const glm::mat4& expected,
        const float tolerance = 1.0e-5f) {
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row)
                EXPECT_NEAR(actual[column][row], expected[column][row], tolerance);
        }
    }
}

TEST(Camera, CachesViewUntilTransformChanges) {
    nk::Camera camera;
    EXPECT_FALSE(camera.dirty());
    expect_matrix_near(camera.view(), glm::mat4{1.0f});

    camera.set_position({2.0f, 3.0f, 4.0f});
    EXPECT_TRUE(camera.dirty());
    const glm::mat4 expected = glm::inverse(glm::translate(
        glm::mat4{1.0f},
        glm::vec3{2.0f, 3.0f, 4.0f}));
    expect_matrix_near(camera.view(), expected);
    EXPECT_FALSE(camera.dirty());
    const glm::mat4 cached = camera.view();
    expect_matrix_near(cached, expected);
    EXPECT_FALSE(camera.dirty());
}

TEST(Camera, ProducesOrthonormalLocalDirections) {
    nk::Camera camera;
    camera.set_euler_rotation({
        glm::radians(20.0f),
        glm::radians(35.0f),
        0.0f,
    });

    const glm::vec3 forward = camera.forward();
    const glm::vec3 right = camera.right();
    const glm::vec3 up = camera.up();
    EXPECT_NEAR(glm::length(forward), 1.0f, 1.0e-5f);
    EXPECT_NEAR(glm::length(right), 1.0f, 1.0e-5f);
    EXPECT_NEAR(glm::length(up), 1.0f, 1.0e-5f);
    EXPECT_NEAR(glm::dot(forward, right), 0.0f, 1.0e-5f);
    EXPECT_NEAR(glm::dot(forward, up), 0.0f, 1.0e-5f);
    EXPECT_NEAR(glm::dot(right, up), 0.0f, 1.0e-5f);
    expect_vec3_near(camera.backward(), -forward);
    expect_vec3_near(camera.left(), -right);
    expect_vec3_near(camera.down(), -up);
}

TEST(Camera, ClampsPitchAndResetsState) {
    nk::Camera camera{{1.0f, 2.0f, 3.0f}};
    camera.pitch(glm::radians(180.0f));
    EXPECT_NEAR(camera.euler_rotation().x, glm::radians(89.0f), 1.0e-5f);
    camera.pitch(glm::radians(-360.0f));
    EXPECT_NEAR(camera.euler_rotation().x, glm::radians(-89.0f), 1.0e-5f);

    camera.reset();
    EXPECT_EQ(camera.position(), glm::vec3(0.0f));
    EXPECT_EQ(camera.euler_rotation(), glm::vec3(0.0f));
    EXPECT_FALSE(camera.dirty());
    expect_matrix_near(camera.view(), glm::mat4{1.0f});
}
