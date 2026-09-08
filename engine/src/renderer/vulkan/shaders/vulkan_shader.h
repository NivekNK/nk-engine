#pragma once

#include "collections/arr.h"
#include "core/result.h"
#include "core/strbuf.h"
#include "renderer/renderer_result.h"
#include "renderer/shader.h"
#include "renderer/shader_config.h"
#include "resources/texture.h"
#include "resources/material.h"
#include "vulkan/samplers.h"
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
            u32 frame_count,
            RenderPass* render_pass,
            Device* device,
            mem::Allocator* allocator,
            ResourceSystem* resources,
            VkAllocationCallbacks* vulkan_allocator,
            Texture* default_texture, vk::Samplers* samplers, SamplerHandle default_sampler);
        void shutdown();

        [[nodiscard]] bool initialized() const noexcept {
            return m_device != nullptr;
        }
        [[nodiscard]] result<void, renderer_error> use(
            const CommandBuffer& command_buffer,
            MaterialBlendMode blend_mode = MaterialBlendMode::opaque);
        [[nodiscard]] result<void, renderer_error> bind_globals();
        [[nodiscard]] result<void, renderer_error> bind_instance(
            u32 instance_id);
        [[nodiscard]] result<void, renderer_error> apply_globals(
            const CommandBuffer& command_buffer,
            u32 frame_index);
        [[nodiscard]] result<void, renderer_error> apply_instance(
            const CommandBuffer& command_buffer,
            u32 frame_index,
            bool needs_update);
        [[nodiscard]] result<void, renderer_error> set_uniform(
            const CommandBuffer& command_buffer,
            ShaderUniformHandle uniform,
            ShaderUniformType type,
            const void* data,
            u32 size);
        [[nodiscard]] result<void, renderer_error> set_sampler(
            ShaderUniformHandle uniform,
            TextureBinding binding,
            u32 array_index = 0);
        [[nodiscard]] result<u32, renderer_error> acquire_resources();
        [[nodiscard]] result<void, renderer_error> release_resources(
            u32 instance_id);

        void set_default_texture(Texture* texture) noexcept {
            m_default_texture = texture;
        }

    private:
        static constexpr u32 max_sampler_count = 32;

        struct DescriptorState {
            cl::arr<u32> generations;
            cl::arr<vk::SampledImageKey> images;
        };

        struct SamplerSlot {
            u32 binding = 0;
            u32 array_element = 0;
        };

        struct InstanceState {
            cl::arr<VkDescriptorSet> descriptor_sets;
            DescriptorState uniform_state;
            cl::arr<DescriptorState> sampler_states;
            cl::arr<u64> uploaded_revisions;
            u64 uniform_revision = 1;
        };

        [[nodiscard]] result<void, renderer_error> init_sampler_slots(
            const ShaderConfig& config);
        [[nodiscard]] result<void, renderer_error> init_descriptor_state(
            DescriptorState& state);
        void release_descriptor_state(DescriptorState& state) noexcept;
        void release_instance_state(InstanceState& state) noexcept;
        [[nodiscard]] u32 sampler_slot(
            ShaderScope scope,
            u32 binding,
            u32 array_element) const noexcept;
        [[nodiscard]] Texture* valid_texture(Texture* texture) const noexcept;
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

        Device* m_device = nullptr;
        mem::Allocator* m_allocator = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        Texture* m_default_texture = nullptr;
        vk::Samplers* m_samplers = nullptr;
        SamplerHandle m_default_sampler{};
        strbuf<255> m_name;

        ShaderMetadata m_metadata;
        cl::arr<VulkanShaderStage> m_stages;
        cl::arr<ShaderPushConstantConfig> m_push_constants;
        cl::arr<VkDescriptorSetLayout> m_descriptor_set_layouts;
        cl::arr<VkDescriptorSet> m_global_descriptor_sets;
        cl::arr<bool> m_global_descriptor_written;
        cl::arr<u8> m_global_uniform_data;
        cl::arr<u8> m_instance_uniform_data;
        cl::arr<SamplerSlot> m_global_sampler_slots;
        cl::arr<SamplerSlot> m_instance_sampler_slots;
        cl::arr<TextureBinding> m_global_textures;
        cl::arr<TextureBinding> m_instance_textures;
        cl::arr<DescriptorState> m_global_sampler_states;
        cl::arr<InstanceState> m_instance_states;
        VkDescriptorPool m_descriptor_pool = nullptr;
        Pipeline m_opaque_pipeline;
        Pipeline m_transparent_pipeline;
        Buffer m_global_uniform_buffer;
        Buffer m_instance_uniform_buffer;

        u32 m_frame_count = 0;
        u32 m_max_instances = 0;
        u32 m_global_set_index = numeric::invalid_id;
        u32 m_instance_set_index = numeric::invalid_id;
        u32 m_global_uniform_binding = numeric::invalid_id;
        u32 m_instance_uniform_binding = numeric::invalid_id;
        u64 m_global_uniform_size = 0;
        u64 m_global_uniform_stride = 0;
        u64 m_instance_uniform_size = 0;
        u64 m_instance_uniform_stride = 0;
        u64 m_instance_frame_stride = 0;
        u32 m_bound_instance_id = numeric::invalid_id;
        bool m_globals_bound = false;
        MaterialBlendMode m_blend_mode = MaterialBlendMode::opaque;

        [[nodiscard]] Pipeline& active_pipeline() noexcept {
            return m_blend_mode == MaterialBlendMode::transparent
                ? m_transparent_pipeline
                : m_opaque_pipeline;
        }
        [[nodiscard]] const Pipeline& active_pipeline() const noexcept {
            return m_blend_mode == MaterialBlendMode::transparent
                ? m_transparent_pipeline
                : m_opaque_pipeline;
        }
    };
}
