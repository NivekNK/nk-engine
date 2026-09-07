#pragma once

#include "vulkan/vk.h"
#include "core/result.h"
#include "renderer/renderer_result.h"

namespace nk {
    class Device;

    enum class CommandBufferState {
        Ready,
        Recording,
        InRenderPass,
        RecordingEnded,
        Submitted,
        NotAllocated
    };

    class CommandBuffer {
    public:
        CommandBuffer() = default;
        ~CommandBuffer() { shutdown(); }

        CommandBuffer(const CommandBuffer&) = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;

        CommandBuffer(CommandBuffer&& other);
        CommandBuffer& operator=(CommandBuffer&& other);

        [[nodiscard]] result<void, renderer_error> init(
            VkCommandPool command_pool,
            Device* device,
            bool is_primary,
            bool is_single_use = false);
        void shutdown();

        [[nodiscard]] result<void, renderer_error> renew(
            VkCommandPool command_pool,
            Device* device,
            bool is_primary,
            bool is_single_use);

        [[nodiscard]] result<void, renderer_error> begin(
            bool is_renderpass_continue,
            bool is_simultaneous_use);
        [[nodiscard]] result<void, renderer_error> end();
        [[nodiscard]] result<void, renderer_error> end_single_use(VkQueue queue);

        void reset() { m_state = CommandBufferState::Ready; }
        void set_state(CommandBufferState state) { m_state = state; }
        CommandBufferState state() const noexcept { return m_state; }

        VkCommandBuffer get() const { return m_command_buffer; }
        VkCommandBuffer operator()() { return m_command_buffer; }
        operator VkCommandBuffer() { return m_command_buffer; }

    private:
        Device* m_device = nullptr;
        VkCommandPool m_command_pool = nullptr;

        VkCommandBuffer m_command_buffer = nullptr;
        CommandBufferState m_state = CommandBufferState::NotAllocated;
        bool m_is_single_use = false;
    };
}
