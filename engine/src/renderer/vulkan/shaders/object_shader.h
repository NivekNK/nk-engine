#pragma once

#include "vulkan/vk.h"

#include "vulkan/pipeline.h"

namespace nk {
    class Device;

    struct ShaderStage {
        VkShaderModuleCreateInfo module_create_info;
        VkShaderModule module;
        VkPipelineShaderStageCreateInfo pipeline_create_info;
    };

    class ObjectShader {
    public:
        static constexpr u32 shader_stage_count = 2;

        ObjectShader() = default;
        ~ObjectShader() { shutdown(); }

        ObjectShader(const ObjectShader&) = delete;
        ObjectShader& operator=(const ObjectShader&) = delete;
        ObjectShader(ObjectShader&&) = delete;
        ObjectShader& operator=(ObjectShader&&) = delete;

        void init(u32 width, u32 height, RenderPass* render_pass, Device* device, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        void use();

    private:
        Device* m_device;
        VkAllocationCallbacks* m_vulkan_allocator;

        ShaderStage m_stages[shader_stage_count];
        Pipeline m_pipeline;
    };
}
