#pragma once

#include "vulkan/vk.h"

#include "renderer/global_uniform_object.h"
#include "renderer/shader_config.h"
#include "resources/material.h"
#include "vulkan/command_buffer.h"
#include "vulkan/shaders/vulkan_shader.h"

#include "vulkan/shaders/material_shader_instance_state.h"

#include "collections/dyarr.h"
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

        // Bind to: m_graphics_command_buffers[image_index]
        void use(CommandBuffer* command_buffer);

        void update_global_state(CommandBuffer& command_buffer, u32 image_index);
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
        mem::Allocator* m_allocator = nullptr;
        Texture* m_default_texture = nullptr;
        VulkanShader m_shader;

        u32 m_image_count = 0;

        // Global Uniform Object
        GlobalUniformObject m_global_ubo{};
        MaterialShaderInstanceState m_instance_states[max_material_count]{};
    };
}
