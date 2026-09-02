#pragma once

#include "vulkan/vk.h"

#include "core/result.h"
#include "vulkan/shaders/material_shader.h"

namespace nk {
    class Device;
    class ResourceSystem;

    [[nodiscard]] result<void, shader_error> create_shader_module(
        cstr name,
        cstr type,
        ResourceSystem& resources,
        Device* device,
        VkAllocationCallbacks* allocator,
        VkShaderStageFlagBits stage,
        ShaderStage* out_stage);
}
