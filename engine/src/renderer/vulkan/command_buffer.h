#pragma once

#include "vulkan/vk.h"

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

        void init(VkCommandPool command_pool, Device* device, bool is_primary, bool is_single_use = false);
        void shutdown();

        void renew(VkCommandPool command_pool, Device* device, bool is_primary, bool is_single_use);

        void begin(bool is_renderpass_continue, bool is_simultaneous_use);
        void end();
        void end_single_use(VkQueue queue);

        void reset() { m_state = CommandBufferState::Ready; }
        void set_state(CommandBufferState state) { m_state = state; }

        VkCommandBuffer get() const { return m_command_buffer; }
        VkCommandBuffer operator()() { return m_command_buffer; }
        operator VkCommandBuffer() { return m_command_buffer; }

    private:
        Device* m_device;
        VkCommandPool m_command_pool;

        VkCommandBuffer m_command_buffer;
        CommandBufferState m_state;
        bool m_is_single_use;
    };
}
