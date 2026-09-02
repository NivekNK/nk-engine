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

    result<void, renderer_error> CommandBuffer::init(
        VkCommandPool command_pool,
        Device* device,
        bool is_primary,
        bool is_single_use) {
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
        VkResult result = vkAllocateCommandBuffers(
            m_device->get(), &allocate_info, &m_command_buffer);
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::command_buffer_failed,
                .native_code = static_cast<i32>(result),
            });
        m_state = CommandBufferState::Ready;

        if (m_is_single_use) {
            auto begun = begin(false, false);
            if (!begun) {
                shutdown();
                return err(begun.error());
            }
        }
        return ok();
    }

    void CommandBuffer::shutdown() {
        if (m_command_buffer == nullptr)
            return;

        vkFreeCommandBuffers(m_device->get(), m_command_pool, 1, &m_command_buffer);
        m_command_buffer = nullptr;
        m_state = CommandBufferState::NotAllocated;
    }

    result<void, renderer_error> CommandBuffer::renew(
        VkCommandPool command_pool,
        Device* device,
        bool is_primary,
        bool is_single_use) {
        shutdown();
        return init(command_pool, device, is_primary, is_single_use);
    }

    result<void, renderer_error> CommandBuffer::begin(
        bool is_renderpass_continue,
        bool is_simultaneous_use) {
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

        const VkResult result = vkBeginCommandBuffer(m_command_buffer, &begin_info);
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::command_buffer_failed,
                .native_code = static_cast<i32>(result),
            });
        m_state = CommandBufferState::Recording;
        return ok();
    }

    result<void, renderer_error> CommandBuffer::end() {
        const VkResult result = vkEndCommandBuffer(m_command_buffer);
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::command_buffer_failed,
                .native_code = static_cast<i32>(result),
            });
        m_state = CommandBufferState::RecordingEnded;
        return ok();
    }

    result<void, renderer_error> CommandBuffer::end_single_use(VkQueue queue) {
        auto ended = end();
        if (!ended)
            return err(ended.error());
        if (!m_is_single_use) {
            std::abort();
        }

        // Submit the queue
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &m_command_buffer;
        VkResult result = vkQueueSubmit(queue, 1, &submit_info, nullptr);
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::queue_submit_failed,
                .native_code = static_cast<i32>(result),
            });

        // Wait for it to finish
        result = vkQueueWaitIdle(queue);
        if (result != VK_SUCCESS)
            return err(renderer_error{
                .code = renderer_error_code::device_wait_failed,
                .native_code = static_cast<i32>(result),
            });

        shutdown();
        return ok();
    }
}
