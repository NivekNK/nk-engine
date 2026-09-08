#include <gtest/gtest.h>

#include "math/frustum.h"

#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace {
    constexpr nk::f32 epsilon = 1.0e-4f;

    bool clip_contains(
        const glm::mat4& view_projection,
        const glm::vec3 point) {
        const glm::vec4 clip = view_projection * glm::vec4{point, 1.0f};
        return clip.x >= -clip.w && clip.x <= clip.w &&
               clip.y >= -clip.w && clip.y <= clip.w &&
               clip.z >= -clip.w && clip.z <= clip.w;
    }
}

TEST(Plane, NormalizesCoefficientsAndUsesInsidePositiveDistance) {
    const nk::math::Plane plane =
        nk::math::Plane::from_coefficients({0.0f, 0.0f, 2.0f, -4.0f});
    ASSERT_TRUE(plane.finite);
    EXPECT_NEAR(glm::length(plane.normal), 1.0f, epsilon);
    EXPECT_EQ(plane.normal, glm::vec3(0.0f, 0.0f, 1.0f));
    EXPECT_NEAR(plane.distance, -2.0f, epsilon);
    EXPECT_GT(plane.signed_distance({0.0f, 0.0f, 3.0f}), 0.0f);
    EXPECT_LT(plane.signed_distance({0.0f, 0.0f, 1.0f}), 0.0f);
}

TEST(Aabb, TransformsCenterAndExtentsConservatively) {
    const nk::math::Aabb local = nk::math::Aabb::from_min_max(
        {-1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, 1.0f});
    glm::mat4 model = glm::translate(
        glm::mat4{1.0f},
        glm::vec3{8.0f, 3.0f, -4.0f});
    model = glm::rotate(
        model,
        glm::radians(90.0f),
        glm::vec3{0.0f, 1.0f, 0.0f});
    model = glm::scale(model, glm::vec3{-2.0f, 3.0f, 4.0f});

    const nk::math::Aabb world = local.transformed(model);
    ASSERT_TRUE(world.finite);
    EXPECT_NEAR(world.center.x, 8.0f, epsilon);
    EXPECT_NEAR(world.center.y, 3.0f, epsilon);
    EXPECT_NEAR(world.center.z, -4.0f, epsilon);
    EXPECT_NEAR(world.half_extents.x, 4.0f, epsilon);
    EXPECT_NEAR(world.half_extents.y, 3.0f, epsilon);
    EXPECT_NEAR(world.half_extents.z, 2.0f, epsilon);
}

TEST(Frustum, ClassifiesPointsAgainstTheCurrentClipSpaceConvention) {
    const glm::mat4 projection = glm::perspective(
        glm::radians(75.0f),
        16.0f / 9.0f,
        0.5f,
        50.0f);
    const glm::mat4 view = glm::lookAt(
        glm::vec3{4.0f, 3.0f, 8.0f},
        glm::vec3{0.0f, 1.0f, 0.0f},
        glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 view_projection = projection * view;
    const nk::math::Frustum frustum =
        nk::math::Frustum::from_view_projection(view_projection);
    ASSERT_TRUE(frustum.finite);

    constexpr glm::vec3 points[]{
        {0.0f, 1.0f, 0.0f},
        {4.0f, 3.0f, 7.0f},
        {50.0f, 0.0f, 0.0f},
        {0.0f, 40.0f, 0.0f},
        {-30.0f, 0.0f, -30.0f},
        {4.0f, 3.0f, 9.0f},
    };
    for (const glm::vec3 point : points) {
        EXPECT_EQ(frustum.contains(point), clip_contains(view_projection, point))
            << "point=" << point.x << "," << point.y << "," << point.z;
    }
}

TEST(Frustum, KeepsIntersectingAndTangentVolumesVisible) {
    const glm::mat4 projection = glm::perspective(
        glm::radians(90.0f),
        1.0f,
        1.0f,
        10.0f);
    const nk::math::Frustum frustum =
        nk::math::Frustum::from_view_projection(projection);

    EXPECT_TRUE(frustum.intersects(nk::math::Aabb::from_min_max(
        {-0.5f, -0.5f, -5.5f},
        {0.5f, 0.5f, -4.5f})));
    EXPECT_FALSE(frustum.intersects(nk::math::Aabb::from_min_max(
        {20.0f, -1.0f, -6.0f},
        {22.0f, 1.0f, -4.0f})));
    EXPECT_TRUE(frustum.intersects(nk::math::Aabb::from_min_max(
        {-0.25f, -0.25f, -1.0f},
        {0.25f, 0.25f, -1.0f})));
    EXPECT_TRUE(frustum.intersects_sphere({0.0f, 0.0f, -5.0f}, 1.0f));
    EXPECT_FALSE(frustum.intersects_sphere({0.0f, 0.0f, -12.0f}, 1.0f));
    EXPECT_TRUE(frustum.intersects_sphere({0.0f, 0.0f, -11.0f}, 1.0f));
}

TEST(Frustum, HandlesComposedTransformsAndFailsOpenForNonFiniteInput) {
    const glm::mat4 projection = glm::perspective(
        glm::radians(60.0f),
        2.0f,
        0.1f,
        100.0f);
    const nk::math::Frustum frustum =
        nk::math::Frustum::from_view_projection(projection);
    const nk::math::Aabb local = nk::math::Aabb::from_min_max(
        {-1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, 1.0f});
    const glm::mat4 parent = glm::translate(
        glm::mat4{1.0f},
        glm::vec3{0.0f, 0.0f, -8.0f});
    const glm::mat4 child = glm::rotate(
        glm::scale(glm::mat4{1.0f}, glm::vec3{3.0f, 0.5f, -2.0f}),
        glm::radians(35.0f),
        glm::vec3{0.0f, 1.0f, 0.0f});
    EXPECT_TRUE(frustum.intersects(local.transformed(parent * child)));

    glm::mat4 bad_matrix{1.0f};
    bad_matrix[0][0] = std::numeric_limits<nk::f32>::quiet_NaN();
    const nk::math::Frustum invalid =
        nk::math::Frustum::from_view_projection(bad_matrix);
    EXPECT_FALSE(invalid.finite);
    EXPECT_TRUE(invalid.intersects(local));
    EXPECT_TRUE(frustum.intersects(local.transformed(bad_matrix)));
    EXPECT_TRUE(frustum.intersects_sphere(
        {0.0f, 0.0f, 0.0f},
        std::numeric_limits<nk::f32>::infinity()));
}
