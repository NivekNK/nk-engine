#include "nkpch.h"

#include "vulkan/command_buffer.h"

#include "vulkan/device.h"

namespace nk {
    CommandBuffer::CommandBuffer(CommandBuffer&& other) 
        : m_device{other.m_device},
          m_command_pool{other.m_command_pool},
          m_command_buffer{other.m_command_buffer},
          m_state{other.m_state},
          m_is_single_use{other.m_is_single_use} {
        other.m_device = nullptr;
        other.m_command_pool = nullptr;
        other.m_command_buffer = nullptr;
        other.m_state = CommandBufferState::Ready;
        other.m_is_single_use = false;
    }

    CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) {
        m_device = other.m_device;
        m_command_pool = other.m_command_pool;
        m_command_buffer = other.m_command_buffer;
        m_state = other.m_state;
        m_is_single_use = other.m_is_single_use;

        other.m_device = nullptr;
        other.m_command_pool = nullptr;
        other.m_command_buffer = nullptr;
        other.m_state = CommandBufferState::Ready;
        other.m_is_single_use = false;

        return *this;
    }

    void CommandBuffer::init(VkCommandPool command_pool, Device* device, bool is_primary, bool is_single_use) {
        m_command_pool = command_pool;
        m_device = device;
        m_is_single_use = is_single_use;

        VkCommandBufferAllocateInfo allocate_info = {};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = m_command_pool;
        allocate_info.level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocate_info.commandBufferCount = 1;
        allocate_info.pNext = nullptr;

        m_state = CommandBufferState::NotAllocated;
        VulkanCheck(vkAllocateCommandBuffers(m_device->get(), &allocate_info, &m_command_buffer));
        m_state = CommandBufferState::Ready;

        if (m_is_single_use) {
            begin(false, false);
        }
    }

    void CommandBuffer::shutdown() {
        if (m_command_buffer == nullptr)
            return;

        vkFreeCommandBuffers(m_device->get(), m_command_pool, 1, &m_command_buffer);
        m_command_buffer = nullptr;
        m_state = CommandBufferState::NotAllocated;
    }

    void CommandBuffer::renew(VkCommandPool command_pool, Device* device, bool is_primary, bool is_single_use) {
        shutdown();
        init(command_pool, device, is_primary, is_single_use);
    }

    void CommandBuffer::begin(bool is_renderpass_continue, bool is_simultaneous_use) {
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        if (m_is_single_use) {
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        }
        if (is_renderpass_continue) {
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        }
        if (is_simultaneous_use) {
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        }

        VulkanCheck(vkBeginCommandBuffer(m_command_buffer, &begin_info));
        m_state = CommandBufferState::Recording;
    }

    void CommandBuffer::end() {
        VulkanCheck(vkEndCommandBuffer(m_command_buffer));
        m_state = CommandBufferState::RecordingEnded;
    }

    void CommandBuffer::end_single_use(VkQueue queue) {
        end();
        if (!m_is_single_use) {
            WarnLog("nk::CommandBuffer::end_single_use trying to end non single use Vulkan Command Buffer!");
            return;
        }

        // Submit the queue
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &m_command_buffer;
        VulkanCheck(vkQueueSubmit(queue, 1, &submit_info, nullptr));

        // Wait for it to finish
        VulkanCheck(vkQueueWaitIdle(queue));

        shutdown();
    }
}
