#pragma once

#include "vulkan/vk.h"

#include "vulkan/shaders/object_shader.h"

namespace nk {
    class Device;
    
    bool create_shader_module(cstr name, cstr type, Device* device, VkAllocationCallbacks* allocator, VkShaderStageFlagBits stage, ShaderStage* out_stage);
}
