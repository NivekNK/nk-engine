#pragma once

#include "renderer/renderer.h"

#include "lve_device.hpp"
#include "lve_swap_chain.hpp"
#include "lve_descriptors.hpp"
#include "lve_game_object.hpp"
#include "systems/point_light_system.hpp"
#include "systems/simple_render_system.hpp"

namespace nk {
    class SimpleVulkanRenderer : public Renderer {
    public:
        SimpleVulkanRenderer() = default;
        ~SimpleVulkanRenderer() = default;

        virtual void on_resized(u32 width, u32 height) override;

        virtual void init() override;

        virtual void shutdown() override;

        virtual bool draw_frame(const RenderPacket& packet) override;

        virtual bool begin_frame(f64 delta_time) override;
        virtual bool end_frame(f64 delta_time) override;

        virtual void update_global_state(
            glm::mat4 projection,
            glm::mat4 view,
            glm::vec3 view_position,
            glm::vec4 ambient_color,
            i32 mode) override {}
        virtual void update_object(glm::mat4 model) override {}

        virtual void create_texture(
            cstr name,
            bool auto_release,
            u32 width,
            u32 height,
            u32 channel_count,
            const u8* pixels,
            bool has_transparency,
            Texture* out_texture) override {}
        virtual void destroy_texture(Texture* texture) override {}

    private:
        void begin_swap_chain_render_pass(VkCommandBuffer command_buffer);
        void end_swap_chain_render_pass(VkCommandBuffer command_buffer);
        void create_command_buffers();
        void free_command_buffers();
        void recreate_swap_chain();
        void load_game_objects();

        lve::LveDevice m_device;

        std::unique_ptr<lve::LveSwapChain> m_swap_chain;
        std::vector<VkCommandBuffer> m_command_buffers;

        u32 m_current_image_index;
        int m_current_frame_index = 0;
        bool m_is_frame_started = false;

        VkCommandBuffer m_current_command_buffer;
        bool m_window_resized = false;

        std::unique_ptr<lve::LveDescriptorPool> m_global_pool;
        lve::LveGameObject::Map m_game_objects;

        std::vector<std::unique_ptr<lve::LveBuffer>> m_ubo_buffers;
        std::unique_ptr<lve::LveDescriptorSetLayout> m_global_set_layout;
        std::vector<VkDescriptorSet> m_global_descriptor_sets;
        std::unique_ptr<lve::SimpleRenderSystem> m_simple_render_system;
        std::unique_ptr<lve::PointLightSystem> m_point_light_system;
        lve::LveCamera m_camera;
        lve::LveGameObject m_viewer_object;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_current_time;
    };
}
