#pragma once

#include "vulkan/vk.h"

#include "renderer/global_uniform_object.h"
#include "resources/material.h"
#include "vulkan/pipeline.h"
#include "vulkan/buffer.h"
#include "vulkan/command_buffer.h"
#include "resources/resource_loader.h"

#include "vulkan/shaders/material_shader_instance_state.h"

#include "collections/dyarr.h"
#include "collections/arr.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Device;
    class ResourceSystem;

    enum class shader_error_code : u8 {
        path_format_failed,
        resource_failed,
        invalid_binary,
        module_creation_failed,
        out_of_memory,
    };

    struct shader_error {
        shader_error_code code;
        resource_error resource;
        VkResult native_code;
    };

    struct ShaderStage {
        VkShaderModuleCreateInfo module_create_info;
        VkShaderModule module;
        VkPipelineShaderStageCreateInfo pipeline_create_info;
    };

    enum class ShaderVertexLayout : u8 {
        vertex_3d,
        vertex_2d,
    };

    class MaterialShader {
    public:
        static constexpr u32 shader_stage_count = 2;
        static constexpr u32 max_material_count = 1024;

        MaterialShader() = default;
        ~MaterialShader() { shutdown(); }

        MaterialShader(const MaterialShader&) = delete;
        MaterialShader& operator=(const MaterialShader&) = delete;
        MaterialShader(MaterialShader&&) = delete;
        MaterialShader& operator=(MaterialShader&&) = delete;

        [[nodiscard]] result<void, renderer_error> init(
            cstr name,
            ShaderVertexLayout vertex_layout,
            bool depth_test_enabled,
            u32 width,
            u32 height,
            u32 image_count,
            RenderPass* render_pass,
            Device* device,
            mem::Allocator* allocator,
            ResourceSystem* resources,
            VkAllocationCallbacks* vulkan_allocator,
            Texture* default_texture);
        void shutdown();

        // Bind to: m_graphics_command_buffers[image_index]
        void use(CommandBuffer* command_buffer);

        void update_global_state(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index, f32 delta_time);
        void set_model(CommandBuffer& command_buffer, const glm::mat4& model);
        void apply_material(
            const cl::dyarr<CommandBuffer>& command_buffers,
            u32 image_index,
            Material& material);

        [[nodiscard]] result<void, renderer_error> acquire_resources(
            Material& material);
        void release_resources(Material& material);

        void set_global_ubo(const GlobalUniformObject& global_ubo) { m_global_ubo = global_ubo; }
        void set_default_texture(Texture* texture) noexcept {
            m_default_texture = texture;
        }

    private:
        Device* m_device = nullptr;
        mem::Allocator* m_allocator = nullptr;
        ResourceSystem* m_resources = nullptr;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        Texture* m_default_texture = nullptr;

        ShaderStage m_stages[shader_stage_count]{};
        Pipeline m_pipeline;

        VkDescriptorPool m_global_descriptor_pool = nullptr;
        VkDescriptorSetLayout m_global_descriptor_set_layout = nullptr;

        u32 m_image_count = 0;
        cl::arr<VkDescriptorSet> m_global_descriptor_sets;

        // Global Uniform Object
        GlobalUniformObject m_global_ubo{};

        // Global uniform buffer
        Buffer m_global_uniform_buffer;

        VkDescriptorPool m_object_descriptor_pool = nullptr;
        VkDescriptorSetLayout m_object_descriptor_set_layout = nullptr;
        // Object uniform buffers
        Buffer m_material_uniform_buffer;
        MaterialShaderInstanceState m_instance_states[max_material_count]{};
    };
}
