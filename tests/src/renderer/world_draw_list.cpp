#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"
#include "renderer/world_draw_list.h"
#include "resources/material.h"

#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace {
    nk::Geometry geometry_at(
        nk::Material* material,
        const glm::vec3 minimum = {-1.0f, -1.0f, -1.0f},
        const glm::vec3 maximum = {1.0f, 1.0f, 1.0f}) {
        nk::Geometry geometry{};
        geometry.material = material;
        geometry.min_extents = minimum;
        geometry.max_extents = maximum;
        geometry.center = (minimum + maximum) * 0.5f;
        return geometry;
    }

    nk::Material valid_material(const nk::MaterialBlendMode blend_mode) {
        nk::Material material{};
        material.internal_id = 1;
        material.generation = 1;
        material.shader = {1, 1};
        material.type = nk::MaterialType::world;
        material.blend_mode = blend_mode;
        return material;
    }

    glm::mat4 at(const glm::vec3 position) {
        return glm::translate(glm::mat4{1.0f}, position);
    }
}

TEST(WorldDrawList, CullsPerViewAndPreservesOpaqueSubmissionOrder) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<nk::GeometryRenderData> output;
    ASSERT_TRUE(output.dyarr_init(&allocator, 4));
    nk::Geometry geometry = geometry_at(nullptr);
    const nk::GeometryRenderData candidates[]{
        {.model = at({0.0f, 0.0f, -5.0f}), .geometry = &geometry,
         .pick_id = {1}},
        {.model = at({50.0f, 0.0f, -5.0f}), .geometry = &geometry,
         .pick_id = {2}},
        {.model = at({1.0f, 0.0f, -6.0f}), .geometry = &geometry,
         .pick_id = {3}},
    };
    const nk::math::Frustum frustum =
        nk::math::Frustum::from_view_projection(glm::perspective(
            glm::radians(90.0f), 1.0f, 0.1f, 100.0f));

    auto built = nk::build_world_draw_list(
        {candidates}, frustum, glm::vec3{0.0f}, true, output);
    ASSERT_TRUE(built);
    EXPECT_EQ(built->candidates, 3u);
    EXPECT_EQ(built->visible, 2u);
    EXPECT_EQ(built->culled, 1u);
    ASSERT_EQ(output.length(), 2u);
    EXPECT_EQ(output[0].pick_id.value, 1u);
    EXPECT_EQ(output[1].pick_id.value, 3u);
}

TEST(WorldDrawList, DisablingCullingKeepsEveryCandidate) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<nk::GeometryRenderData> output;
    ASSERT_TRUE(output.dyarr_init(&allocator, 1));
    nk::Geometry geometry = geometry_at(nullptr);
    const nk::GeometryRenderData candidates[]{
        {.model = at({1000.0f, 0.0f, 10.0f}), .geometry = &geometry},
    };

    auto built = nk::build_world_draw_list(
        {candidates}, {}, glm::vec3{0.0f}, false, output);
    ASSERT_TRUE(built);
    EXPECT_EQ(built->visible, 1u);
    EXPECT_EQ(built->culled, 0u);
}

TEST(WorldDrawList, SortsOnlyVisibleTransparentGeometryBackToFront) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<nk::GeometryRenderData> output;
    ASSERT_TRUE(output.dyarr_init(&allocator, 5));
    nk::Material opaque = valid_material(nk::MaterialBlendMode::opaque);
    nk::Material transparent = valid_material(
        nk::MaterialBlendMode::transparent);
    nk::Geometry opaque_geometry = geometry_at(&opaque);
    nk::Geometry transparent_geometry = geometry_at(&transparent);
    const nk::GeometryRenderData candidates[]{
        {.model = at({0.0f, 0.0f, -4.0f}),
         .geometry = &transparent_geometry, .pick_id = {1}},
        {.model = at({0.0f, 0.0f, -3.0f}),
         .geometry = &opaque_geometry, .pick_id = {2}},
        {.model = at({0.0f, 0.0f, -12.0f}),
         .geometry = &transparent_geometry, .pick_id = {3}},
        {.model = at({80.0f, 0.0f, -8.0f}),
         .geometry = &transparent_geometry, .pick_id = {4}},
        {.model = at({0.0f, 0.0f, -4.0f}),
         .geometry = &transparent_geometry, .pick_id = {5}},
    };
    const nk::math::Frustum frustum =
        nk::math::Frustum::from_view_projection(glm::perspective(
            glm::radians(90.0f), 1.0f, 0.1f, 100.0f));

    auto built = nk::build_world_draw_list(
        {candidates}, frustum, glm::vec3{0.0f}, true, output);
    ASSERT_TRUE(built);
    ASSERT_EQ(output.length(), 4u);
    EXPECT_EQ(output[0].pick_id.value, 2u);
    EXPECT_EQ(output[1].pick_id.value, 3u);
    EXPECT_EQ(output[2].pick_id.value, 1u);
    EXPECT_EQ(output[3].pick_id.value, 5u);
}

TEST(WorldDrawList, KeepsInvalidBoundsVisibleAndReusesReservedStorage) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<nk::GeometryRenderData> output;
    ASSERT_TRUE(output.dyarr_init(&allocator, 2));
    nk::Geometry geometry = geometry_at(nullptr);
    glm::mat4 invalid{1.0f};
    invalid[0][0] = std::numeric_limits<nk::f32>::quiet_NaN();
    const nk::GeometryRenderData candidates[]{
        {.model = invalid, .geometry = &geometry},
        {.model = at({0.0f, 0.0f, -5.0f}), .geometry = &geometry},
    };
    const nk::math::Frustum frustum =
        nk::math::Frustum::from_view_projection(glm::perspective(
            glm::radians(90.0f), 1.0f, 0.1f, 100.0f));

    auto first = nk::build_world_draw_list(
        {candidates}, frustum, glm::vec3{0.0f}, true, output);
    ASSERT_TRUE(first);
    ASSERT_EQ(first->visible, 2u);
    const nk::u64 capacity = output.capacity();
    auto second = nk::build_world_draw_list(
        {candidates}, frustum, glm::vec3{0.0f}, true, output);
    ASSERT_TRUE(second);
    EXPECT_EQ(output.capacity(), capacity);
}
