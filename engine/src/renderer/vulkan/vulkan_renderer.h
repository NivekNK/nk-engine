#pragma once

#include "renderer/renderer.h"

#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "collections/dyarr.h"
#include "vulkan/framebuffer.h"
#include "vulkan/command_buffer.h"
#include "vulkan/fence.h"
#include "vulkan/buffer.h"
#include "vulkan/host_allocator.h"
#include "vulkan/samplers.h"
#include "memory/malloc_allocator.h"
#include "memory/synchronized_allocator.h"
#include "renderer/render_target.h"

#include "vulkan/shaders/vulkan_shader.h"

namespace nk {
    class VulkanRenderer : public Renderer {
    public:
        VulkanRenderer(
            mem::Allocator& allocator,
            ResourceSystem* resources,
            const strview application_name)
            : Renderer{allocator, resources, application_name} {}
        ~VulkanRenderer() = default;

        virtual void on_resized(u32 width, u32 height) override;
        f64 gpu_frame_ms() const noexcept override { return m_gpu_frame_ms; }

        [[nodiscard]] virtual result<ShaderHandle, renderer_error> create_shader(
            const ShaderConfig& config,
            RenderPassKind render_pass) override;
        [[nodiscard]] virtual result<void, renderer_error> destroy_shader(
            ShaderHandle shader) override;
        [[nodiscard]] virtual result<void, renderer_error> use_shader(
            ShaderHandle shader) override;
        [[nodiscard]] virtual result<void, renderer_error> bind_shader_globals(
            ShaderHandle shader) override;
        [[nodiscard]] virtual result<void, renderer_error> bind_shader_instance(
            ShaderHandle shader,
            u32 instance_id) override;
        [[nodiscard]] virtual result<void, renderer_error> apply_shader_globals(
            ShaderHandle shader) override;
        [[nodiscard]] virtual result<void, renderer_error> apply_shader_instance(
            ShaderHandle shader,
            bool needs_update) override;
        [[nodiscard]] virtual result<u32, renderer_error>
        acquire_shader_instance(ShaderHandle shader) override;
        [[nodiscard]] virtual result<void, renderer_error>
        release_shader_instance(
            ShaderHandle shader,
            u32 instance_id) override;
        [[nodiscard]] virtual result<void, renderer_error> set_shader_sampler(
            ShaderHandle shader,
            ShaderUniformHandle uniform,
            TextureBinding binding,
            u32 array_index = 0) override;
        [[nodiscard]] result<SamplerHandle, renderer_error> create_sampler(const SamplerConfig& config) override;
        [[nodiscard]] result<void, renderer_error> release_sampler(SamplerHandle sampler) override;

        virtual result<void, renderer_error> init() override;
        virtual void shutdown() override;
        virtual result<frame_outcome, renderer_error> begin_frame(
            f64 delta_time) override;
        virtual void begin_render_pass(RenderPassKind pass) override;
        virtual void end_render_pass(RenderPassKind pass) override;
        virtual void draw_geometry(
            RenderPassKind pass,
            GeometryRenderData data) override;
        virtual result<frame_outcome, renderer_error> end_frame(
            f64 delta_time) override;
    
        virtual result<void, renderer_error> create_texture(
            strview name,
            u32 width,
            u32 height,
            u32 channel_count,
            const u8* pixels,
            bool has_transparency,
            Texture* out_texture) override;
        [[nodiscard]] result<void, renderer_error> create_texture_cube(
            strview name,
            u32 width,
            u32 height,
            u32 channel_count,
            const u8* face_pixels,
            Texture* out_texture) override;
        [[nodiscard]] result<void, renderer_error>
        create_writable_texture(Texture* texture) override;
        [[nodiscard]] result<void, renderer_error> write_texture(
            Texture& texture,
            TextureRegion region,
            cl::slice<const u8> pixels) override;
        [[nodiscard]] result<void, renderer_error> resize_texture(
            Texture& texture,
            u32 width,
            u32 height) override;
        virtual void destroy_texture(Texture* texture) override;
        virtual result<void, renderer_error> create_geometry(
            Geometry& geometry,
            cl::slice<const glm::Vertex3D> vertices,
            cl::slice<const u32> indices) override;
        virtual result<void, renderer_error> create_geometry(
            Geometry& geometry,
            cl::slice<const glm::Vertex2D> vertices,
            cl::slice<const u32> indices) override;
        virtual void destroy_geometry(Geometry& geometry) override;

    private:
        [[nodiscard]] result<void, renderer_error> set_material_blend_mode(
            MaterialBlendMode blend_mode) override;
        bool sampler_mutation_allowed() const noexcept;
        void on_default_texture_changed(Texture* texture) override;
        [[nodiscard]] result<void, renderer_error> set_shader_uniform_raw(
            ShaderHandle shader,
            ShaderUniformHandle uniform,
            ShaderUniformType type,
            const void* data,
            u32 size) override;
        [[nodiscard]] result<void, renderer_error> recreate_render_targets();
        [[nodiscard]] result<void, renderer_error> recreate_command_buffers();
        [[nodiscard]] result<void, renderer_error> recreate_sync_objects();
        [[nodiscard]] result<void, renderer_error> recreate_swapchain();
        [[nodiscard]] result<void, renderer_error> create_buffers();
        [[nodiscard]] result<void, renderer_error> create_geometry_internal(
            Geometry& geometry,
            u64 vertex_stride,
            u64 vertex_alignment,
            u64 vertex_count,
            const void* vertices,
            cl::slice<const u32> indices);
        [[nodiscard]] result<void, renderer_error> upload_geometry_ranges(
            const RenderBufferView& vertices,
            const void* vertex_data,
            const RenderBufferView& indices,
            const void* index_data);

        struct VulkanGeometryData {
            u32 id = numeric::invalid_id;
            u32 generation = numeric::invalid_id;
            u64 vertex_count = 0;
            RenderBufferView vertex_range{};
            u64 index_count = 0;
            RenderBufferView index_range{};
        };

        bool release_geometry_ranges(
            const VulkanGeometryData& geometry) noexcept;
        [[nodiscard]] result<void, renderer_error>
        wait_for_in_flight_frames() noexcept;

        struct VulkanShaderSlot {
            VulkanShader* shader = nullptr;
            u16 generation = 0;
            RenderPassKind render_pass = RenderPassKind::world;
        };

        [[nodiscard]] VulkanShader* resolve_shader(
            ShaderHandle handle) noexcept;
        void destroy_all_shaders() noexcept;

        static constexpr u32 max_geometry_count = 4096;
        static constexpr u16 max_shader_count = 16;
        static constexpr u64 geometry_range_capacity =
            static_cast<u64>(max_geometry_count) + 2;

        mem::MallocAllocator m_vulkan_host_backing{mem::untracked};
        mem::SynchronizedAllocator m_vulkan_host_memory;
        vk::VulkanHostAllocator m_vulkan_host_allocator;
        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        Instance m_instance;
        Device m_device;
        vk::Samplers m_samplers;
        SamplerHandle m_default_sampler{};
        Swapchain m_swapchain;
        RenderPass m_world_render_pass;
        RenderPass m_ui_render_pass;
        RenderAttachmentConfig m_world_attachment_configs[2]{};
        RenderAttachmentConfig m_ui_attachment_configs[1]{};
        RenderPassConfig m_world_render_pass_config{};
        RenderPassConfig m_ui_render_pass_config{};
        cl::dyarr<RenderTarget> m_world_targets;
        cl::dyarr<RenderTarget> m_ui_targets;
        cl::dyarr<Framebuffer> m_world_framebuffers;
        cl::dyarr<Framebuffer> m_ui_framebuffers;
        cl::dyarr<CommandBuffer> m_graphics_command_buffers;
        VkQueryPool m_timestamp_pool = VK_NULL_HANDLE;
        cl::arr<bool> m_timestamp_pending;
        f64 m_gpu_frame_ms = -1.0;

        // Per-frame semaphores for synchronization
        cl::dyarr<VkSemaphore> m_image_available_semaphores;
        cl::dyarr<VkSemaphore> m_queue_complete_semaphores;
        cl::dyarr<Fence> m_in_flight_fences;
        cl::dyarr<Fence*> m_images_in_flight;

        u32 m_framebuffer_width = 0;
        u32 m_framebuffer_height = 0;
        u32 m_cached_framebuffer_width = 0;
        u32 m_cached_framebuffer_height = 0;

        // Current genration of frambuffer size. If it does not match m_framebuffer_last_generation
        // a new one should be generated.
        u64 m_framebuffer_size_generation = 0;

        // The generation of the framebuffer when it was last created. Set to m_framebuffer_size_generation
        // when updated.
        u64 m_framebuffer_last_generation = 0;

        u32 m_image_index = 0;
        u32 m_current_frame = 0;

        VulkanShaderSlot m_shaders[max_shader_count]{};
        ShaderHandle m_active_shader;
        RenderPassKind m_active_render_pass = RenderPassKind::world;
        bool m_render_pass_active = false;

        // Buffers
        Buffer m_object_vertex_buffer;
        Buffer m_object_index_buffer;

        VulkanGeometryData m_geometries[max_geometry_count]{};

        f32 m_frame_delta_time = 0.0f;

        bool m_instance_initialized = false;
        bool m_device_initialized = false;
        bool m_swapchain_initialized = false;
        bool m_world_render_pass_initialized = false;
        bool m_ui_render_pass_initialized = false;
    };
}
