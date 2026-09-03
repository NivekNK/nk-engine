#pragma once

#include "collections/arr.h"
#include "core/result.h"
#include "core/strbuf.h"
#include "renderer/renderer_result.h"
#include "renderer/shader_config.h"
#include "vulkan/buffer.h"
#include "vulkan/pipeline.h"
#include "vulkan/shaders/utils.h"

namespace nk {
    class CommandBuffer;
    class Device;
    class RenderPass;
    class ResourceSystem;

    class VulkanShader {
    public:
        VulkanShader() = default;
        ~VulkanShader() { shutdown(); }

        VulkanShader(const VulkanShader&) = delete;
        VulkanShader& operator=(const VulkanShader&) = delete;
        VulkanShader(VulkanShader&&) = delete;
        VulkanShader& operator=(VulkanShader&&) = delete;

        [[nodiscard]] result<void, renderer_error> init(
            const ShaderConfig& config,
            u32 width,
            u32 height,
            u32 image_count,
            RenderPass* render_pass,
            Device* device,
            mem::Allocator* allocator,
            ResourceSystem* resources,
            VkAllocationCallbacks* vulkan_allocator);
        void shutdown();

        [[nodiscard]] bool initialized() const noexcept {
            return m_device != nullptr;
        }
        void use(const CommandBuffer& command_buffer);
        [[nodiscard]] result<void, renderer_error> apply_global_uniform(
            const CommandBuffer& command_buffer,
            u32 image_index,
            const void* data,
            u64 size);
        [[nodiscard]] result<void, renderer_error> load_instance_uniform(
            u32 instance_id,
            const void* data,
            u64 size);
        [[nodiscard]] result<void, renderer_error> push_constant(
            const CommandBuffer& command_buffer,
            ShaderStage stages,
            u32 offset,
            u32 size,
            const void* data) const;
        [[nodiscard]] result<void, renderer_error> push_constant(
            const CommandBuffer& command_buffer,
            u32 offset,
            u32 size,
            const void* data) const;

        [[nodiscard]] result<void, renderer_error>
        allocate_instance_descriptor_sets(cl::arr<VkDescriptorSet>& sets);
        [[nodiscard]] result<void, renderer_error>
        release_instance_descriptor_sets(cl::arr<VkDescriptorSet>& sets);
        void bind_instance_descriptor_set(
            const CommandBuffer& command_buffer,
            VkDescriptorSet set) const;
        void update_descriptors(
            u32 write_count,
            const VkWriteDescriptorSet* writes) const;

        [[nodiscard]] VkBuffer instance_uniform_buffer() const noexcept {
            return m_instance_uniform_buffer.get();
        }
        [[nodiscard]] u64 global_uniform_size() const noexcept {
            return m_global_uniform_size;
        }
        [[nodiscard]] u64 instance_uniform_size() const noexcept {
            return m_instance_uniform_size;
        }
        [[nodiscard]] u64 instance_uniform_stride() const noexcept {
            return m_instance_uniform_stride;
        }
        [[nodiscard]] u32 instance_uniform_binding() const noexcept {
            return m_instance_uniform_binding;
        }
        [[nodiscard]] u32 instance_sampler_binding() const noexcept {
            return m_instance_sampler_binding;
        }
        [[nodiscard]] u32 image_count() const noexcept {
            return m_image_count;
        }
        [[nodiscard]] u32 max_instances() const noexcept {
            return m_max_instances;
        }

    private:
        Device* m_device = nullptr;
        mem::Allocator* m_allocator = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        strbuf<255> m_name;

        cl::arr<VulkanShaderStage> m_stages;
        cl::arr<ShaderPushConstantConfig> m_push_constants;
        cl::arr<VkDescriptorSetLayout> m_descriptor_set_layouts;
        cl::arr<VkDescriptorSet> m_global_descriptor_sets;
        VkDescriptorPool m_descriptor_pool = nullptr;
        Pipeline m_pipeline;
        Buffer m_global_uniform_buffer;
        Buffer m_instance_uniform_buffer;

        u32 m_image_count = 0;
        u32 m_max_instances = 0;
        u32 m_global_set_index = numeric::invalid_id;
        u32 m_instance_set_index = numeric::invalid_id;
        u32 m_global_uniform_binding = numeric::invalid_id;
        u32 m_instance_uniform_binding = numeric::invalid_id;
        u32 m_instance_sampler_binding = numeric::invalid_id;
        u64 m_global_uniform_size = 0;
        u64 m_instance_uniform_size = 0;
        u64 m_instance_uniform_stride = 0;
    };
}
