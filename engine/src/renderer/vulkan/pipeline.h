#pragma once

#include "vulkan/vk.h"

namespace nk {
    class Device;
    class RenderPass;
    class CommandBuffer;

    struct PipelineCreateInfo {
        Device* device;
        VkAllocationCallbacks* vulkan_allocator;
        RenderPass* render_pass;
        u32 attribute_count;
        VkVertexInputAttributeDescription* attributes;
        u32 descriptor_set_layout_count;
        VkDescriptorSetLayout* descriptor_set_layouts;
        u32 stage_count;
        VkPipelineShaderStageCreateInfo* stages;
        VkViewport viewport;
        VkRect2D scissor;
        bool is_wireframe;
    };

    class Pipeline {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(Pipeline&&) = delete;

        void init(const PipelineCreateInfo& create_info);
        void shutdown();

        void bind(CommandBuffer* command_buffer, VkPipelineBindPoint bind_point);

    private:
        Device* m_device;
        VkAllocationCallbacks* m_vulkan_allocator;

        VkPipeline m_pipeline;
        VkPipelineLayout m_layout;
    };
}
