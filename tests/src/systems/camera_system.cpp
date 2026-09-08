#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"
#include "systems/camera_system.h"

namespace {
    class CameraSystemTest : public testing::Test {
    protected:
        void TearDown() override {
            if (system != nullptr)
                nk::CameraSystem::destroy(allocator, system);
        }

        nk::mem::MallocAllocator allocator{nk::mem::untracked};
        nk::CameraSystem* system = nullptr;
    };
}

TEST_F(CameraSystemTest, CreatesDefaultAndSharesNamedCameras) {
    auto created = nk::CameraSystem::create(allocator, 3);
    ASSERT_TRUE(created);
    system = *created;
    EXPECT_EQ(nk::CameraSystem::current(), system);
    ASSERT_NE(nk::Camera::active(), nullptr);
    EXPECT_EQ(system->loaded_count(), 1u);
    EXPECT_EQ(system->reference_count(nk::default_camera_name), 1u);

    auto first = system->acquire("editor");
    auto second = system->acquire("editor");
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(*first, *second);
    EXPECT_EQ(system->reference_count("editor"), 2u);
    EXPECT_EQ(system->loaded_count(), 2u);

    ASSERT_TRUE(system->set_active(*first));
    EXPECT_EQ(system->active_handle(), *first);
    EXPECT_EQ(nk::Camera::active(), system->get(*first));
    EXPECT_TRUE(system->release(*first));
    EXPECT_EQ(system->reference_count("editor"), 1u);
    EXPECT_TRUE(system->release(*second));
    EXPECT_EQ(system->active_handle(), system->default_handle());
    EXPECT_EQ(system->loaded_count(), 1u);
}

TEST_F(CameraSystemTest, RejectsCapacityAndStaleHandlesTransactionally) {
    auto created = nk::CameraSystem::create(allocator, 2);
    ASSERT_TRUE(created);
    system = *created;

    auto first = system->acquire("first");
    ASSERT_TRUE(first);
    auto full = system->acquire("second");
    ASSERT_FALSE(full);
    EXPECT_EQ(full.error().code, nk::camera_error_code::capacity_exceeded);
    EXPECT_EQ(system->loaded_count(), 2u);

    ASSERT_TRUE(system->release(*first));
    EXPECT_EQ(system->get(*first), nullptr);
    auto reused = system->acquire("second");
    ASSERT_TRUE(reused);
    EXPECT_EQ(reused->index, first->index);
    EXPECT_NE(reused->generation, first->generation);

    auto stale = system->set_active(*first);
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error().code, nk::camera_error_code::invalid_handle);
    EXPECT_EQ(system->active_handle(), system->default_handle());
}

TEST_F(CameraSystemTest, ProtectsTheDefaultCameraAndGlobalLifetime) {
    auto created = nk::CameraSystem::create(allocator, 1);
    ASSERT_TRUE(created);
    system = *created;

    auto released = system->release(system->default_handle());
    ASSERT_FALSE(released);
    EXPECT_EQ(
        released.error().code,
        nk::camera_error_code::default_camera_release);
    EXPECT_NE(nk::Camera::active(), nullptr);

    nk::CameraSystem::destroy(allocator, system);
    system = nullptr;
    EXPECT_EQ(nk::CameraSystem::current(), nullptr);
    EXPECT_EQ(nk::Camera::active(), nullptr);
}
