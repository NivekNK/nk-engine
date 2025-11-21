#pragma once

#include "vulkan/vk.h"

#include "vulkan/pipeline.h"

#include "renderer/global_uniform_object.h"
#include "vulkan/buffer.h"
#include "vulkan/command_buffer.h"

#include "collections/dyarr.h"

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
        static constexpr u32 descriptor_count = 1;
        static constexpr u32 object_max_object_count = 1024;

        ObjectShader() = default;
        ~ObjectShader() { shutdown(); }

        ObjectShader(const ObjectShader&) = delete;
        ObjectShader& operator=(const ObjectShader&) = delete;
        ObjectShader(ObjectShader&&) = delete;
        ObjectShader& operator=(ObjectShader&&) = delete;

        void init(u32 width, u32 height, RenderPass* render_pass, Device* device, VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        // Bind to: m_graphics_command_buffers[image_index]
        void use(CommandBuffer* command_buffer);

        void update_global_state(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index);
        void update_object(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index, glm::mat4 model);

        void set_global_ubo(const GlobalUniformObject& global_ubo) { m_global_ubo = global_ubo; }

    private:
        Device* m_device;
        VkAllocationCallbacks* m_vulkan_allocator;

        ShaderStage m_stages[shader_stage_count];
        Pipeline m_pipeline;

        VkDescriptorPool m_global_descriptor_pool;
        VkDescriptorSetLayout m_global_descriptor_set_layout;

        // One descriptor set per frame - max 3 for triple buffering
        static constexpr u32 global_descriptor_set_count = 3;
        VkDescriptorSet m_global_descriptor_sets[global_descriptor_set_count];

        // Global Uniform Object
        GlobalUniformObject m_global_ubo;

        // Global uniform buffer
        Buffer m_global_uniform_buffer;
    };
}
