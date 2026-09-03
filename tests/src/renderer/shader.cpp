#include <gtest/gtest.h>

#include <type_traits>

#include "memory/malloc_allocator.h"
#include "renderer/shader.h"

namespace {
    class FailingAllocator final : public nk::mem::MallocAllocator {
    public:
        FailingAllocator() : MallocAllocator{nk::mem::untracked} {}

    protected:
        void* _do_allocate(nk::u64, nk::u64) noexcept override {
            return nullptr;
        }
    };

    nk::ShaderConfig valid_config(
        const nk::cl::slice<const nk::ShaderUniformConfig> uniforms) {
        static const nk::ShaderStageConfig stages[] = {
            {nk::ShaderStage::vertex, "vertex", "main"},
            {nk::ShaderStage::fragment, "fragment", "main"},
        };
        static const nk::ShaderAttributeConfig attributes[] = {
            {"position", nk::ShaderAttributeType::f32, 0, 0},
        };
        static const nk::ShaderDescriptorBindingConfig bindings[] = {
            {
                0,
                nk::ShaderDescriptorType::uniform_buffer,
                1,
                nk::ShaderStage::vertex,
                131072,
            },
        };
        static const nk::ShaderDescriptorSetConfig sets[] = {
            {nk::ShaderScope::global, bindings},
        };
        return {
            .name = "test",
            .stages = stages,
            .attributes = attributes,
            .descriptor_sets = sets,
            .uniforms = uniforms,
            .push_constants = {},
            .vertex_stride = 4,
        };
    }
}

static_assert(std::is_trivially_copyable_v<nk::ShaderHandle>);
static_assert(std::is_trivially_copyable_v<nk::ShaderUniformHandle>);
static_assert(nk::shader_uniform_type<glm::mat4>() ==
              nk::ShaderUniformType::mat4);
static_assert(nk::shader_uniform_type<glm::vec4>() ==
              nk::ShaderUniformType::f32_4);

TEST(ShaderMetadata, StoresCheckedCompactUniforms) {
    const nk::ShaderUniformConfig uniforms[] = {
        {
            "custom",
            nk::ShaderUniformType::custom,
            nk::ShaderScope::global,
            0,
            64,
            257,
        },
    };
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::ShaderMetadata metadata;

    auto initialized = metadata.init(&allocator, valid_config(uniforms));

    ASSERT_TRUE(initialized);
    ASSERT_EQ(metadata.uniform_count(), 1);
    const auto* uniform = metadata.uniform({0});
    ASSERT_NE(uniform, nullptr);
    EXPECT_EQ(uniform->offset, 64);
    EXPECT_EQ(uniform->size, 257);
    EXPECT_EQ(uniform->binding, 0);
    EXPECT_EQ(uniform->array_length, 1);
    EXPECT_EQ(uniform->type, nk::ShaderUniformType::custom);
    EXPECT_EQ(uniform->scope, nk::ShaderScope::global);
    EXPECT_EQ(metadata.uniform({1}), nullptr);
}

TEST(ShaderMetadata, PreservesUniformArrayLengthAndTotalSize) {
    const nk::ShaderUniformConfig uniforms[] = {
        {
            .name = "values",
            .type = nk::ShaderUniformType::custom,
            .scope = nk::ShaderScope::global,
            .binding = 0,
            .offset = 16,
            .custom_size = 12,
            .array_length = 3,
        },
    };
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::ShaderMetadata metadata;

    auto initialized = metadata.init(&allocator, valid_config(uniforms));

    ASSERT_TRUE(initialized);
    const auto* uniform = metadata.uniform({0});
    ASSERT_NE(uniform, nullptr);
    EXPECT_EQ(uniform->offset, 16u);
    EXPECT_EQ(uniform->size, 36u);
    EXPECT_EQ(uniform->array_length, 3u);
}

TEST(ShaderMetadata, RejectsWidthsThatCannotBeRepresented) {
    const nk::ShaderUniformConfig uniforms[] = {
        {
            "oversized",
            nk::ShaderUniformType::custom,
            nk::ShaderScope::global,
            0,
            0,
            65536,
        },
    };
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::ShaderMetadata metadata;

    auto initialized = metadata.init(&allocator, valid_config(uniforms));

    ASSERT_FALSE(initialized);
    EXPECT_EQ(
        initialized.error().code,
        nk::renderer_error_code::shader_config_invalid);
    EXPECT_EQ(
        initialized.error().native_code,
        static_cast<nk::i32>(
            nk::shader_config_error::metadata_limits_exceeded));
    EXPECT_FALSE(metadata.initialized());
}

TEST(ShaderMetadata, AllocationFailureLeavesNoPartialState) {
    const nk::ShaderUniformConfig uniforms[] = {
        {
            "value",
            nk::ShaderUniformType::f32,
            nk::ShaderScope::global,
            0,
            0,
            0,
        },
    };
    FailingAllocator allocator;
    nk::ShaderMetadata metadata;

    auto initialized = metadata.init(&allocator, valid_config(uniforms));

    ASSERT_FALSE(initialized);
    EXPECT_EQ(
        initialized.error().code,
        nk::renderer_error_code::out_of_memory);
    EXPECT_FALSE(metadata.initialized());
}
