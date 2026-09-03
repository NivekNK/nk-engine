#pragma once

#include "vulkan/vk.h"

#include "renderer/shader.h"
#include "resources/texture.h"
#include "vulkan/command_buffer.h"
#include "vulkan/shaders/vulkan_shader.h"

#include "vulkan/shaders/material_shader_instance_state.h"

#include "collections/arr.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Device;
    class ResourceSystem;

    class MaterialShader {
    public:
        static constexpr u32 max_material_count = 1024;

        MaterialShader() = default;
        ~MaterialShader() { shutdown(); }

        MaterialShader(const MaterialShader&) = delete;
        MaterialShader& operator=(const MaterialShader&) = delete;
        MaterialShader(MaterialShader&&) = delete;
        MaterialShader& operator=(MaterialShader&&) = delete;

        [[nodiscard]] result<void, renderer_error> init(
            const ShaderConfig& config,
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

        [[nodiscard]] result<void, renderer_error> use(
            CommandBuffer& command_buffer);
        [[nodiscard]] result<void, renderer_error> bind_globals();
        [[nodiscard]] result<void, renderer_error> bind_instance(
            u32 instance_id);
        [[nodiscard]] result<void, renderer_error> apply_globals(
            CommandBuffer& command_buffer,
            u32 image_index);
        [[nodiscard]] result<void, renderer_error> apply_instance(
            CommandBuffer& command_buffer,
            u32 image_index);
        [[nodiscard]] result<void, renderer_error> set_uniform(
            CommandBuffer& command_buffer,
            ShaderUniformHandle uniform,
            ShaderUniformType type,
            const void* data,
            u32 size);
        [[nodiscard]] result<void, renderer_error> set_sampler(
            ShaderUniformHandle uniform,
            Texture* texture);

        [[nodiscard]] result<u32, renderer_error> acquire_resources();
        [[nodiscard]] result<void, renderer_error> release_resources(
            u32 instance_id);

        void set_default_texture(Texture* texture) noexcept {
            m_default_texture = texture;
        }

    private:
        mem::Allocator* m_allocator = nullptr;
        Texture* m_default_texture = nullptr;
        VulkanShader m_shader;
        ShaderMetadata m_metadata;
        cl::arr<u8> m_global_uniform_data;
        cl::arr<u8> m_instance_uniform_data;

        u32 m_image_count = 0;
        u32 m_max_instances = 0;
        u32 m_bound_instance_id = numeric::invalid_id;
        u64 m_instance_uniform_size = 0;
        bool m_globals_bound = false;
        MaterialShaderInstanceState m_instance_states[max_material_count]{};
        Texture* m_instance_textures[max_material_count]{};
    };
}
