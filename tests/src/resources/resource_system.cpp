#include <type_traits>

#include <gtest/gtest.h>

#include "collections/dyarr.h"
#include "core/str.h"
#include "memory/malloc_allocator.h"
#include "resources/image_loader.h"
#include "resources/material.h"
#include "systems/resource_system.h"

static_assert(!std::is_copy_constructible_v<nk::Resource>);
static_assert(std::is_move_constructible_v<nk::Resource>);

TEST(ResourceSystem, LoadsAndExplicitlyUnloadsKnownResourceTypes) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(created);
    nk::ResourceSystem* resources = *created;
    EXPECT_EQ(resources->registered_loader_count(), 4u);

    auto text = resources->load(
        "materials/test_material.kmt",
        nk::ResourceType::text);
    ASSERT_TRUE(text);
    ASSERT_NE(text->as<nk::str>(), nullptr);
    EXPECT_NE(text->as<nk::str>()->view().find("diffuse_map_name"), nk::strview::npos);
    EXPECT_TRUE(text->full_path.view().ends_with("materials/test_material.kmt"));

    auto binary = resources->load(
        "models/cube.obj",
        nk::ResourceType::binary);
    ASSERT_TRUE(binary);
    ASSERT_NE(binary->as<nk::cl::dyarr<nk::u8>>(), nullptr);
    EXPECT_GT(binary->data_size, 0u);

    auto image = resources->load("cobblestone", nk::ResourceType::image);
    ASSERT_TRUE(image);
    ASSERT_NE(image->as<nk::DecodedImage>(), nullptr);
    EXPECT_EQ(image->as<nk::DecodedImage>()->width, 512u);
    EXPECT_EQ(image->as<nk::DecodedImage>()->height, 512u);

    auto material = resources->load(
        "test_material",
        nk::ResourceType::material);
    ASSERT_TRUE(material);
    ASSERT_NE(material->as<nk::MaterialConfig>(), nullptr);
    EXPECT_EQ(
        material->as<nk::MaterialConfig>()->diffuse_map_name.view(),
        nk::strview{"paving"});
    EXPECT_EQ(resources->active_resource_count(), 4u);

    EXPECT_TRUE(resources->unload(*material));
    EXPECT_TRUE(resources->unload(*image));
    EXPECT_TRUE(resources->unload(*binary));
    EXPECT_TRUE(resources->unload(*text));
    EXPECT_EQ(resources->active_resource_count(), 0u);
    EXPECT_FALSE(material->loaded());

    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(ResourceSystem, RejectsMissingAndDuplicateLoadersWithoutPublishingData) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(created);
    nk::ResourceSystem* resources = *created;

    auto missing = resources->load("missing", nk::ResourceType::image);
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, nk::resource_error_code::file_failed);
    EXPECT_EQ(resources->active_resource_count(), 0u);

    auto unsupported = resources->load("mesh", nk::ResourceType::static_mesh);
    ASSERT_FALSE(unsupported);
    EXPECT_EQ(unsupported.error().code, nk::resource_error_code::no_loader);

    nk::TextResourceLoader duplicate;
    auto registered = resources->register_loader(duplicate);
    ASSERT_FALSE(registered);
    EXPECT_EQ(
        registered.error().code,
        nk::resource_error_code::duplicate_loader);

    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}
