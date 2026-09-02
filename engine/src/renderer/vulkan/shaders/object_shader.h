#pragma once

#include "vulkan/vk.h"

#include "renderer/global_uniform_object.h"
#include "renderer/geometry_render_data.h"
#include "vulkan/pipeline.h"
#include "vulkan/buffer.h"
#include "vulkan/command_buffer.h"
#include "platform/file.h"

#include "vulkan/shaders/object_shader_object_state.h"

#include "collections/dyarr.h"
#include "collections/arr.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Device;

    enum class shader_error_code : u8 {
        path_format_failed,
        file_failed,
        invalid_binary,
        module_creation_failed,
        out_of_memory,
    };

    struct shader_error {
        shader_error_code code;
        file_error file;
        VkResult native_code;
    };

    struct ShaderStage {
        VkShaderModuleCreateInfo module_create_info;
        VkShaderModule module;
        VkPipelineShaderStageCreateInfo pipeline_create_info;
    };

    class ObjectShader {
    public:
        static constexpr u32 shader_stage_count = 2;
        static constexpr u32 object_max_object_count = 1024;

        ObjectShader() = default;
        ~ObjectShader() { shutdown(); }

        ObjectShader(const ObjectShader&) = delete;
        ObjectShader& operator=(const ObjectShader&) = delete;
        ObjectShader(ObjectShader&&) = delete;
        ObjectShader& operator=(ObjectShader&&) = delete;

        [[nodiscard]] result<void, renderer_error> init(
            u32 width,
            u32 height,
            u32 image_count,
            RenderPass* render_pass,
            Device* device,
            mem::Allocator* allocator,
            VkAllocationCallbacks* vulkan_allocator,
            Texture* default_texture);
        void shutdown();

        // Bind to: m_graphics_command_buffers[image_index]
        void use(CommandBuffer* command_buffer);

        void update_global_state(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index, f32 delta_time);
        void update_object(const cl::dyarr<CommandBuffer>& command_buffers, u32 image_index, GeometryRenderData data, f32 delta_time);

        bool acquire_resources(u32* out_object_id);
        void release_resources(u32 object_id);

        void set_global_ubo(const GlobalUniformObject& global_ubo) { m_global_ubo = global_ubo; }

    private:
        Device* m_device = nullptr;
        mem::Allocator* m_allocator = nullptr;
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
        Buffer m_object_uniform_buffer;
        // TODO: manage a free list of som kind here instead.
        u32 m_object_uniform_buffer_index = 0;

        // TODO: make dynamic
        ObjectShaderObjectState m_object_states[object_max_object_count]{};
    };
}
