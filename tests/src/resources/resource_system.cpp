#include <type_traits>

#include <gtest/gtest.h>

#include "collections/dyarr.h"
#include "core/str.h"
#include "memory/malloc_allocator.h"
#include "resources/image_loader.h"
#include "resources/material.h"
#include "resources/shader_resource.h"
#include "resources/static_mesh_resource.h"
#include "systems/material_system.h"
#include "systems/resource_system.h"

static_assert(!std::is_copy_constructible_v<nk::Resource>);
static_assert(std::is_move_constructible_v<nk::Resource>);

TEST(ResourceSystem, ParsesPerMapSamplingAndPreservesLegacyDefaults) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::ResourceSystem::create(allocator, NK_TEST_FIXTURE_ROOT);
    ASSERT_TRUE(created);
    auto* resources = *created;
    auto loaded = resources->load("sampling", nk::ResourceType::material);
    ASSERT_TRUE(loaded);
    const auto& config = *loaded->as<nk::MaterialConfig>();
    EXPECT_EQ(config.blend_mode, nk::MaterialBlendMode::masked);
    EXPECT_EQ(config.diffuse_sampler.min_filter, nk::TextureFilter::nearest);
    EXPECT_EQ(config.diffuse_sampler.mag_filter, nk::TextureFilter::linear);
    EXPECT_EQ(config.diffuse_sampler.mip_filter, nk::TextureFilter::nearest);
    EXPECT_EQ(config.diffuse_sampler.wrap_u, nk::TextureWrap::mirrored_repeat);
    EXPECT_EQ(config.diffuse_sampler.wrap_v, nk::TextureWrap::clamp_to_edge);
    EXPECT_EQ(config.diffuse_sampler.wrap_w, nk::TextureWrap::clamp_to_border);
    EXPECT_EQ(config.diffuse_sampler.anisotropy, 1);
    EXPECT_EQ(config.specular_sampler.anisotropy, 8);
    EXPECT_EQ(config.normal_sampler, nk::SamplerConfig{});
    EXPECT_TRUE(resources->unload(*loaded));
    for (nk::strview name : {"invalid_sampling", "invalid_anisotropy"}) {
        auto invalid = resources->load(name, nk::ResourceType::material);
        ASSERT_FALSE(invalid);
        EXPECT_EQ(invalid.error().code, nk::resource_error_code::invalid_data);
    }
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(ResourceSystem, LoadsAndExplicitlyUnloadsKnownResourceTypes) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_ASSET_ROOT);
    ASSERT_TRUE(created);
    nk::ResourceSystem* resources = *created;
    EXPECT_EQ(resources->registered_loader_count(), 6u);

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
    EXPECT_EQ(
        material->as<nk::MaterialConfig>()->specular_map_name.view(),
        nk::strview{"paving_SPEC"});
    EXPECT_EQ(
        material->as<nk::MaterialConfig>()->normal_map_name.view(),
        nk::strview{"paving_NRM"});
    EXPECT_FLOAT_EQ(material->as<nk::MaterialConfig>()->shininess, 64.0f);
    EXPECT_EQ(
        material->as<nk::MaterialConfig>()->type,
        nk::MaterialType::world);
    EXPECT_EQ(
        material->as<nk::MaterialConfig>()->shader_name.view(),
        nk::strview{"Builtin.MaterialShader"});

    auto ui_material = resources->load(
        "test_ui_material",
        nk::ResourceType::material);
    ASSERT_TRUE(ui_material);
    EXPECT_EQ(
        ui_material->as<nk::MaterialConfig>()->type,
        nk::MaterialType::ui);
    EXPECT_EQ(
        ui_material->as<nk::MaterialConfig>()->blend_mode,
        nk::MaterialBlendMode::transparent);
    EXPECT_EQ(
        ui_material->as<nk::MaterialConfig>()->diffuse_map_name.view(),
        nk::strview{"orange_lines_512"});
    EXPECT_EQ(
        ui_material->as<nk::MaterialConfig>()->shader_name.view(),
        nk::strview{"Builtin.UIShader"});

    auto shader = resources->load(
        "Builtin.MaterialShader",
        nk::ResourceType::shader);
    ASSERT_TRUE(shader);
    const nk::ShaderResourceConfig* shader_resource =
        shader->as<nk::ShaderResourceConfig>();
    ASSERT_NE(shader_resource, nullptr);
    EXPECT_EQ(shader_resource->render_pass(), nk::RenderPassKind::world);
    const nk::ShaderConfig shader_config = shader_resource->config();
    EXPECT_EQ(shader_config.name, nk::strview{"Builtin.MaterialShader"});
    ASSERT_EQ(shader_config.stages.length(), 2u);
    ASSERT_EQ(shader_config.attributes.length(), 4u);
    EXPECT_EQ(shader_config.attributes[1].offset, 12u);
    EXPECT_EQ(shader_config.attributes[2].offset, 24u);
    EXPECT_EQ(shader_config.attributes[3].offset, 32u);
    EXPECT_EQ(shader_config.vertex_stride, 48u);
    ASSERT_EQ(shader_config.descriptor_sets.length(), 2u);
    ASSERT_EQ(shader_config.descriptor_sets[0].bindings.length(), 1u);
    EXPECT_EQ(shader_config.descriptor_sets[0].bindings[0].element_size, 292u);
    ASSERT_EQ(shader_config.descriptor_sets[1].bindings.length(), 2u);
    EXPECT_EQ(shader_config.descriptor_sets[1].bindings[0].element_size, 28u);
    EXPECT_EQ(shader_config.descriptor_sets[1].bindings[1].count, 3u);
    ASSERT_EQ(shader_config.uniforms.length(), 18u);
    EXPECT_EQ(shader_config.uniforms[1].offset, 64u);
    EXPECT_EQ(shader_config.uniforms[3].type, nk::ShaderUniformType::sampler_2d);
    EXPECT_EQ(shader_config.uniforms[3].offset, 0u);
    EXPECT_EQ(shader_config.uniforms[3].array_length, 1u);
    EXPECT_EQ(shader_config.uniforms[5].offset, 128u);
    EXPECT_EQ(shader_config.uniforms[6].offset, 144u);
    EXPECT_EQ(shader_config.uniforms[7].offset, 160u);
    ASSERT_EQ(shader_config.push_constants.length(), 1u);
    EXPECT_EQ(shader_config.uniforms[8].offset, 64u);
    EXPECT_EQ(
        shader_config.uniforms[9].type,
        nk::ShaderUniformType::sampler_2d);
    EXPECT_EQ(shader_config.uniforms[9].offset, 1u);
    EXPECT_EQ(shader_config.uniforms[10].offset, 16u);
    EXPECT_EQ(shader_config.uniforms[11].type, nk::ShaderUniformType::u32);
    EXPECT_EQ(shader_config.uniforms[11].offset, 20u);
    EXPECT_EQ(shader_config.uniforms[12].type, nk::ShaderUniformType::f32);
    EXPECT_EQ(shader_config.uniforms[12].offset, 24u);
    EXPECT_EQ(shader_config.uniforms[13].offset, 176u);
    EXPECT_EQ(
        shader_config.uniforms[14].type,
        nk::ShaderUniformType::sampler_2d);
    EXPECT_EQ(shader_config.uniforms[14].offset, 2u);
    EXPECT_EQ(shader_config.uniforms[15].type, nk::ShaderUniformType::u32);
    EXPECT_EQ(shader_config.uniforms[15].offset, 188u);
    EXPECT_EQ(shader_config.uniforms[16].type, nk::ShaderUniformType::custom);
    EXPECT_EQ(shader_config.uniforms[16].offset, 192u);
    EXPECT_EQ(shader_config.uniforms[16].custom_size, 48u);
    EXPECT_EQ(shader_config.uniforms[16].array_length, 2u);
    EXPECT_EQ(nk::shader_uniform_size(shader_config.uniforms[16]), 96u);
    EXPECT_EQ(shader_config.uniforms[17].type, nk::ShaderUniformType::u32);
    EXPECT_EQ(shader_config.uniforms[17].offset, 288u);
    EXPECT_EQ(shader_config.push_constants[0].size, 128u);
    EXPECT_EQ(resources->active_resource_count(), 6u);

    EXPECT_TRUE(resources->unload(*shader));
    EXPECT_TRUE(resources->unload(*ui_material));
    EXPECT_TRUE(resources->unload(*material));
    EXPECT_TRUE(resources->unload(*image));
    EXPECT_TRUE(resources->unload(*binary));
    EXPECT_TRUE(resources->unload(*text));
    EXPECT_EQ(resources->active_resource_count(), 0u);
    EXPECT_FALSE(material->loaded());

    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(ResourceSystem, ParsesShaderArraysAndReportsTypedConfigErrors) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_FIXTURE_ROOT);
    ASSERT_TRUE(created);
    nk::ResourceSystem* resources = *created;

    auto array_shader = resources->load(
        "Test.Array",
        nk::ResourceType::shader);
    ASSERT_TRUE(array_shader);
    const nk::ShaderConfig config =
        array_shader->as<nk::ShaderResourceConfig>()->config();
    ASSERT_EQ(config.uniforms.length(), 2u);
    EXPECT_EQ(config.uniforms[0].type, nk::ShaderUniformType::custom);
    EXPECT_EQ(config.uniforms[0].array_length, 3u);
    EXPECT_EQ(nk::shader_uniform_size(config.uniforms[0]), 48u);
    EXPECT_EQ(config.uniforms[1].array_length, 3u);
    EXPECT_EQ(config.uniforms[1].offset, 0u);
    ASSERT_EQ(config.descriptor_sets.length(), 1u);
    ASSERT_EQ(config.descriptor_sets[0].bindings.length(), 2u);
    EXPECT_EQ(config.descriptor_sets[0].bindings[1].count, 3u);
    EXPECT_TRUE(resources->unload(*array_shader));

    auto invalid = resources->load(
        "Test.Invalid",
        nk::ResourceType::shader);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, nk::resource_error_code::invalid_data);
    EXPECT_EQ(
        invalid.error().native_code,
        static_cast<nk::i32>(
            nk::shader_resource_parse_error::invalid_uniform));
    EXPECT_EQ(resources->active_resource_count(), 0u);

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

    auto missing_mesh = resources->load(
        "missing",
        nk::ResourceType::static_mesh);
    ASSERT_FALSE(missing_mesh);
    EXPECT_EQ(
        missing_mesh.error().code,
        nk::resource_error_code::file_failed);

    nk::TextResourceLoader duplicate;
    auto registered = resources->register_loader(duplicate);
    ASSERT_FALSE(registered);
    EXPECT_EQ(
        registered.error().code,
        nk::resource_error_code::duplicate_loader);

    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(ResourceSystem, LoadsTriangulatesAndGroupsObjMeshesTransactionally) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_FIXTURE_ROOT);
    ASSERT_TRUE(created);
    nk::ResourceSystem* resources = *created;

    auto loaded = resources->load(
        "mesh_features",
        nk::ResourceType::static_mesh);
    ASSERT_TRUE(loaded);
    const nk::StaticMeshResource* mesh =
        loaded->as<nk::StaticMeshResource>();
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->geometries.length(), 2u);
    ASSERT_EQ(mesh->materials.length(), 2u);

    const nk::GeometryConfig& quad = mesh->geometries[0];
    EXPECT_EQ(quad.indices.length(), 6u);
    EXPECT_EQ(quad.vertices.length(), 4u);
    EXPECT_EQ(quad.material_name.view(), nk::strview{"red"});
    EXPECT_EQ(quad.min_extents, glm::vec3(-2.0f, 0.0f, -1.0f));
    EXPECT_EQ(quad.max_extents, glm::vec3(2.0f, 0.0f, 1.0f));
    EXPECT_EQ(quad.center, glm::vec3(0.0f));
    for (const glm::Vertex3D& vertex : quad.vertices) {
        EXPECT_NE(vertex.normal, glm::vec3(0.0f));
        EXPECT_NE(glm::vec3(vertex.tangent), glm::vec3(0.0f));
    }

    const nk::GeometryConfig& triangle = mesh->geometries[1];
    EXPECT_EQ(triangle.indices.length(), 3u);
    EXPECT_EQ(triangle.vertices.length(), 3u);
    EXPECT_EQ(triangle.material_name.view(), nk::strview{"blue"});
    EXPECT_EQ(triangle.vertices[0].normal, glm::vec3(0.0f, 1.0f, 0.0f));
    EXPECT_FLOAT_EQ(mesh->materials[0].shininess, 16.0f);
    EXPECT_FLOAT_EQ(mesh->materials[1].shininess, 32.0f);

    EXPECT_TRUE(resources->unload(*loaded));
    EXPECT_EQ(resources->active_resource_count(), 0u);
    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(ResourceSystem, GeneratesMissingObjNormalsAndRejectsMalformedInput) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::ResourceSystem::create(
        allocator,
        NK_TEST_FIXTURE_ROOT);
    ASSERT_TRUE(created);
    nk::ResourceSystem* resources = *created;

    auto loaded = resources->load(
        "no_material",
        nk::ResourceType::static_mesh);
    ASSERT_TRUE(loaded);
    const nk::StaticMeshResource* mesh =
        loaded->as<nk::StaticMeshResource>();
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->geometries.length(), 1u);
    EXPECT_EQ(mesh->materials.length(), 0u);
    EXPECT_EQ(
        mesh->geometries[0].material_name.view(),
        nk::default_material_name);
    for (const glm::Vertex3D& vertex : mesh->geometries[0].vertices) {
        EXPECT_EQ(vertex.normal, glm::vec3(0.0f, 0.0f, 1.0f));
        EXPECT_NE(glm::vec3(vertex.tangent), glm::vec3(0.0f));
    }
    EXPECT_TRUE(resources->unload(*loaded));

    auto invalid = resources->load(
        "invalid",
        nk::ResourceType::static_mesh);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, nk::resource_error_code::invalid_data);
    EXPECT_EQ(
        invalid.error().native_code,
        static_cast<nk::i32>(nk::static_mesh_parse_error::invalid_index));
    EXPECT_EQ(resources->active_resource_count(), 0u);

    nk::ResourceSystem::destroy(allocator, resources);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}
