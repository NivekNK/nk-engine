#pragma once

#include "vulkan/vk.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

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
        u32 push_constant_range_count;
        VkPushConstantRange* push_constant_ranges;
        u32 stage_count;
        VkPipelineShaderStageCreateInfo* stages;
        VkViewport viewport;
        VkRect2D scissor;
        u32 vertex_stride;
        bool is_wireframe;
        bool depth_test_enabled;
    };

    class Pipeline {
    public:
        Pipeline() = default;
        ~Pipeline() { shutdown(); }

        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(Pipeline&&) = delete;

        [[nodiscard]] result<void, renderer_error> init(
            const PipelineCreateInfo& create_info);
        void shutdown();

        void bind(CommandBuffer* command_buffer, VkPipelineBindPoint bind_point);

        VkPipelineLayout get_layout() const { return m_layout; }

    private:
        Device* m_device = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;

        VkPipeline m_pipeline = nullptr;
        VkPipelineLayout m_layout = nullptr;
    };
}
