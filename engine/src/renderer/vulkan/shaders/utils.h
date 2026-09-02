#pragma once

#include "vulkan/vk.h"

#include "core/result.h"
#include "platform/file.h"
#include "vulkan/shaders/object_shader.h"

namespace nk {
    class Device;

    [[nodiscard]] result<void, shader_error> create_shader_module(
        cstr name,
        cstr type,
        Device* device,
        VkAllocationCallbacks* allocator,
        VkShaderStageFlagBits stage,
        ShaderStage* out_stage);
}
