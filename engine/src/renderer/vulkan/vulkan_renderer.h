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
#include "vulkan/shaders/object_shader.h"

namespace nk {
    class VulkanRenderer : public Renderer {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() = default;

        virtual void on_resized(u32 width, u32 height) override;

        virtual void init() override;
        virtual void shutdown() override;
        virtual bool begin_frame(f64 delta_time) override;
        virtual void update_global_state(
            glm::mat4 projection,
            glm::mat4 view,
            glm::vec3 view_position,
            glm::vec4 ambient_color,
            i32 mode) override;
        virtual void update_object(glm::mat4 model) override;
        virtual bool end_frame(f64 delta_time) override;
    
        virtual void create_texture(
            cstr name,
            bool auto_release,
            u32 width,
            u32 height,
            u32 channel_count,
            const u8* pixels,
            bool has_transparency,
            Texture* out_texture) override;
        virtual void destroy_texture(Texture* texture) override;

    private:
        void recreate_framebuffers();
        void recreate_command_buffers();
        void recreate_sync_objects();
        void recreate_swapchain();
        void create_buffers();

        void upload_data_range(
            VkCommandPool pool,
            VkFence fence,
            VkQueue queue,
            Buffer* buffer,
            u64 offset,
            u64 size,
            void* data
        );

        VkAllocationCallbacks* m_vulkan_allocator;
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

        u32 m_framebuffer_width;
        u32 m_framebuffer_height;
        u32 m_cached_framebuffer_width;
        u32 m_cached_framebuffer_height;

        // Current genration of frambuffer size. If it does not match m_framebuffer_last_generation
        // a new one should be generated.
        u64 m_framebuffer_size_generation;

        // The generation of the framebuffer when it was last created. Set to m_framebuffer_size_generation
        // when updated.
        u64 m_framebuffer_last_generation;

        u32 m_image_index;
        u32 m_current_frame;

        // Shaders
        ObjectShader m_object_shader;

        // Buffers
        Buffer m_object_vertex_buffer;
        Buffer m_object_index_buffer;

        u64 m_geometry_vertex_offset;
        u64 m_geometry_index_offset;
    };
}
