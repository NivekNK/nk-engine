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

// Shaders
#include "vulkan/shaders/material_shader.h"

namespace nk {
    class VulkanRenderer : public Renderer {
    public:
        VulkanRenderer(mem::Allocator& allocator, const strview application_name)
            : Renderer{allocator, application_name} {}
        ~VulkanRenderer() = default;

        virtual void on_resized(u32 width, u32 height) override;

        virtual result<void, renderer_error> init() override;
        virtual void shutdown() override;
        virtual result<frame_outcome, renderer_error> begin_frame(
            f64 delta_time) override;
        virtual void update_global_state(
            glm::mat4 projection,
            glm::mat4 view,
            glm::vec3 view_position,
            glm::vec4 ambient_color,
            i32 mode) override;
        virtual void draw_geometry(GeometryRenderData data) override;
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
        virtual void destroy_texture(Texture* texture) override;
        virtual result<void, renderer_error> create_material(
            Material& material) override;
        virtual void destroy_material(Material& material) override;
        virtual result<void, renderer_error> create_geometry(
            Geometry& geometry,
            cl::slice<const glm::Vertex3D> vertices,
            cl::slice<const u32> indices) override;
        virtual void destroy_geometry(Geometry& geometry) override;

    private:
        void on_default_texture_changed(Texture* texture) override {
            m_material_shader.set_default_texture(texture);
        }
        [[nodiscard]] result<void, renderer_error> recreate_framebuffers();
        [[nodiscard]] result<void, renderer_error> recreate_command_buffers();
        [[nodiscard]] result<void, renderer_error> recreate_sync_objects();
        [[nodiscard]] result<void, renderer_error> recreate_swapchain();
        [[nodiscard]] result<void, renderer_error> create_buffers();

        [[nodiscard]] result<void, renderer_error> upload_data_range(
            VkCommandPool pool,
            VkFence fence,
            VkQueue queue,
            Buffer* buffer,
            u64 offset,
            u64 size,
            const void* data
        );

        struct VulkanGeometryData {
            u32 id = numeric::invalid_id;
            u32 generation = numeric::invalid_id;
            u64 vertex_count = 0;
            u64 vertex_size = 0;
            u64 vertex_buffer_offset = 0;
            u64 index_count = 0;
            u64 index_size = 0;
            u64 index_buffer_offset = 0;
        };

        static constexpr u32 max_geometry_count = 4096;

        VkAllocationCallbacks* m_vulkan_allocator = nullptr;
        Instance m_instance;
        Device m_device;
        Swapchain m_swapchain;
        RenderPass m_main_render_pass;
        cl::dyarr<Framebuffer> m_framebuffers;
        cl::dyarr<CommandBuffer> m_graphics_command_buffers;

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

        // Shaders
        MaterialShader m_material_shader;

        // Buffers
        Buffer m_object_vertex_buffer;
        Buffer m_object_index_buffer;

        u64 m_geometry_vertex_offset = 0;
        u64 m_geometry_index_offset = 0;
        VulkanGeometryData m_geometries[max_geometry_count]{};

        f32 m_frame_delta_time = 0.0f;

        bool m_instance_initialized = false;
        bool m_device_initialized = false;
        bool m_swapchain_initialized = false;
        bool m_render_pass_initialized = false;
    };
}
