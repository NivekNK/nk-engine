#include <gtest/gtest.h>

#include "core/math.h"

#include <glm/gtc/matrix_transform.hpp>

TEST(Math, SafelyNormalizesFiniteAndDegenerateVectors) {
    EXPECT_EQ(nk::math::safe_normalize(glm::vec3{0.0f}), glm::vec3{0.0f});
    EXPECT_EQ(
        nk::math::safe_normalize(glm::vec3{0.0f, 0.0f, 4.0f}),
        glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(
        nk::math::safe_normalize(glm::vec3{1.0e30f, 0.0f, 0.0f}),
        glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST(Math, TransformsNormalsWithInverseTransposeForNonUniformScale) {
    const glm::mat4 model = glm::scale(
        glm::mat4{1.0f},
        glm::vec3{2.0f, 1.0f, 3.0f});
    const glm::vec3 tangent{1.0f, -1.0f, 0.0f};
    const glm::vec3 normal{1.0f, 1.0f, 0.0f};

    const glm::vec3 transformed_tangent = glm::mat3{model} * tangent;
    const glm::vec3 transformed_normal =
        nk::math::transform_normal(model, normal);

    EXPECT_NEAR(glm::dot(transformed_tangent, transformed_normal), 0.0f, 1.0e-6f);
    EXPECT_NEAR(glm::dot(transformed_normal, transformed_normal), 1.0f, 1.0e-6f);
    EXPECT_EQ(
        nk::math::transform_normal(
            glm::scale(glm::mat4{1.0f}, glm::vec3{1.0f, 0.0f, 1.0f}),
            normal),
        glm::vec3{0.0f});
    EXPECT_NE(
        nk::math::transform_normal(
            glm::scale(glm::mat4{1.0f}, glm::vec3{1.0e-4f}),
            normal),
        glm::vec3{0.0f});
}
