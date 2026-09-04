#include <gtest/gtest.h>

#include "core/transform.h"

#include <concepts>
#include <utility>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {
    void expect_matrix_near(
        const glm::mat4& actual,
        const glm::mat4& expected,
        const float tolerance = 1.0e-5f) {
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                EXPECT_NEAR(
                    actual[column][row],
                    expected[column][row],
                    tolerance);
            }
        }
    }
}

static_assert(!std::copy_constructible<nk::Transform>);
static_assert(std::move_constructible<nk::Transform>);
static_assert(std::is_nothrow_move_constructible_v<nk::Transform>);

TEST(Transform, StartsAtIdentityAndCachesLocalAndWorldMatrices) {
    nk::Transform transform;

    EXPECT_FALSE(transform.local_dirty());
    EXPECT_FALSE(transform.world_dirty());
    expect_matrix_near(transform.local_matrix(), glm::mat4{1.0f});
    expect_matrix_near(transform.world_matrix(), glm::mat4{1.0f});

    transform.set_position({2.0f, 3.0f, 4.0f});
    EXPECT_TRUE(transform.local_dirty());
    EXPECT_TRUE(transform.world_dirty());

    const glm::mat4 expected = glm::translate(
        glm::mat4{1.0f},
        glm::vec3{2.0f, 3.0f, 4.0f});
    expect_matrix_near(transform.local_matrix(), expected);
    EXPECT_FALSE(transform.local_dirty());
    EXPECT_TRUE(transform.world_dirty());
    expect_matrix_near(transform.world_matrix(), expected);
    EXPECT_FALSE(transform.world_dirty());
}

TEST(Transform, ComposesTranslationRotationAndScale) {
    const glm::vec3 position{2.0f, -3.0f, 4.0f};
    const glm::quat rotation = glm::angleAxis(
        glm::radians(90.0f),
        glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::vec3 scale{2.0f, 3.0f, 4.0f};
    nk::Transform transform{position, rotation, scale};

    const glm::mat4 expected =
        glm::translate(glm::mat4{1.0f}, position) *
        glm::mat4_cast(rotation) *
        glm::scale(glm::mat4{1.0f}, scale);
    expect_matrix_near(transform.local_matrix(), expected);

    transform.translate({1.0f, 2.0f, 3.0f});
    transform.rotate(glm::angleAxis(
        glm::radians(15.0f),
        glm::vec3{1.0f, 0.0f, 0.0f}));
    transform.scale_by({0.5f, 2.0f, 1.0f});
    EXPECT_EQ(transform.position(), glm::vec3(3.0f, -1.0f, 7.0f));
    EXPECT_EQ(transform.scale(), glm::vec3(1.0f, 6.0f, 4.0f));
    EXPECT_NEAR(glm::length(transform.rotation()), 1.0f, 1.0e-6f);
}

TEST(Transform, PropagatesWorldInvalidationThroughHierarchy) {
    nk::Transform parent{{10.0f, 0.0f, 0.0f}};
    nk::Transform child{{0.0f, 5.0f, 0.0f}};
    nk::Transform grandchild{{0.0f, 0.0f, 2.0f}};
    ASSERT_TRUE(child.set_parent(&parent));
    ASSERT_TRUE(grandchild.set_parent(&child));

    const glm::mat4 expected = glm::translate(
        glm::mat4{1.0f},
        glm::vec3{10.0f, 5.0f, 2.0f});
    expect_matrix_near(grandchild.world_matrix(), expected);
    EXPECT_FALSE(parent.world_dirty());
    EXPECT_FALSE(child.world_dirty());
    EXPECT_FALSE(grandchild.world_dirty());

    parent.translate({1.0f, 0.0f, 0.0f});
    EXPECT_TRUE(parent.world_dirty());
    EXPECT_TRUE(child.world_dirty());
    EXPECT_TRUE(grandchild.world_dirty());
    expect_matrix_near(
        grandchild.world_matrix(),
        glm::translate(
            glm::mat4{1.0f},
            glm::vec3{11.0f, 5.0f, 2.0f}));
}

TEST(Transform, RejectsSelfParentingAndCyclesWithoutChangingHierarchy) {
    nk::Transform root;
    nk::Transform child;
    nk::Transform grandchild;
    ASSERT_TRUE(child.set_parent(&root));
    ASSERT_TRUE(grandchild.set_parent(&child));

    auto self_parent = root.set_parent(&root);
    ASSERT_FALSE(self_parent);
    EXPECT_EQ(
        self_parent.error().code,
        nk::transform_error_code::self_parent);
    EXPECT_EQ(root.parent(), nullptr);

    auto cycle = root.set_parent(&grandchild);
    ASSERT_FALSE(cycle);
    EXPECT_EQ(cycle.error().code, nk::transform_error_code::cycle);
    EXPECT_EQ(root.parent(), nullptr);
    EXPECT_EQ(child.parent(), &root);
    EXPECT_EQ(grandchild.parent(), &child);
}

TEST(Transform, ReparentsAndDetachesWithoutAllocatingHierarchyStorage) {
    nk::Transform first_parent{{1.0f, 0.0f, 0.0f}};
    nk::Transform second_parent{{5.0f, 0.0f, 0.0f}};
    nk::Transform child{{0.0f, 2.0f, 0.0f}};
    ASSERT_TRUE(child.set_parent(&first_parent));
    EXPECT_EQ(first_parent.child_count(), 1u);
    expect_matrix_near(
        child.world_matrix(),
        glm::translate(glm::mat4{1.0f}, glm::vec3{1.0f, 2.0f, 0.0f}));

    ASSERT_TRUE(child.set_parent(&second_parent));
    EXPECT_EQ(first_parent.child_count(), 0u);
    EXPECT_EQ(second_parent.child_count(), 1u);
    expect_matrix_near(
        child.world_matrix(),
        glm::translate(glm::mat4{1.0f}, glm::vec3{5.0f, 2.0f, 0.0f}));

    child.clear_parent();
    EXPECT_EQ(child.parent(), nullptr);
    EXPECT_EQ(second_parent.child_count(), 0u);
    expect_matrix_near(
        child.world_matrix(),
        glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 2.0f, 0.0f}));
}

TEST(Transform, RepairsHierarchyLinksWhenMoved) {
    nk::Transform parent{{3.0f, 0.0f, 0.0f}};
    nk::Transform child{{0.0f, 4.0f, 0.0f}};
    nk::Transform grandchild{{0.0f, 0.0f, 5.0f}};
    ASSERT_TRUE(child.set_parent(&parent));
    ASSERT_TRUE(grandchild.set_parent(&child));

    nk::Transform moved_child{std::move(child)};
    EXPECT_EQ(parent.first_child(), &moved_child);
    EXPECT_EQ(moved_child.parent(), &parent);
    EXPECT_EQ(grandchild.parent(), &moved_child);
    expect_matrix_near(
        grandchild.world_matrix(),
        glm::translate(glm::mat4{1.0f}, glm::vec3{3.0f, 4.0f, 5.0f}));
}

TEST(Transform, DetachesChildrenBeforeParentLifetimeEnds) {
    nk::Transform child{{0.0f, 2.0f, 0.0f}};
    {
        nk::Transform parent{{4.0f, 0.0f, 0.0f}};
        ASSERT_TRUE(child.set_parent(&parent));
        expect_matrix_near(
            child.world_matrix(),
            glm::translate(
                glm::mat4{1.0f},
                glm::vec3{4.0f, 2.0f, 0.0f}));
    }

    EXPECT_EQ(child.parent(), nullptr);
    EXPECT_TRUE(child.world_dirty());
    expect_matrix_near(
        child.world_matrix(),
        glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 2.0f, 0.0f}));
}
