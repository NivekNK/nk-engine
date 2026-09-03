#pragma once

#include "vulkan/vk.h"

#include "core/strbuf.h"
#include "core/result.h"
#include "renderer/renderer_result.h"
#include "renderer/shader_config.h"

namespace nk {
    class Device;
    class ResourceSystem;

    struct VulkanShaderStage {
        VkShaderModuleCreateInfo module_create_info{};
        VkShaderModule module = nullptr;
        VkPipelineShaderStageCreateInfo pipeline_create_info{};
        strbuf<63> entry_point;
    };

    [[nodiscard]] result<void, renderer_error> create_shader_module(
        strview name,
        const ShaderStageConfig& config,
        ResourceSystem& resources,
        Device* device,
        VkAllocationCallbacks* allocator,
        VulkanShaderStage* out_stage);
}
