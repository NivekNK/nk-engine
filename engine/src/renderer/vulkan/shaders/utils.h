#pragma once

#include "vulkan/vk.h"

#include "core/result.h"
#include "platform/file.h"
#include "vulkan/shaders/object_shader.h"

namespace nk {
    class Device;

    enum class shader_error_code : u8 {
        path_format_failed,
        file_failed,
        invalid_binary,
        module_creation_failed,
    };

    struct shader_error {
        shader_error_code code;
        file_error file;
        VkResult native_code;
    };

    [[nodiscard]] result<void, shader_error> create_shader_module(
        cstr name,
        cstr type,
        Device* device,
        VkAllocationCallbacks* allocator,
        VkShaderStageFlagBits stage,
        ShaderStage* out_stage);
}
