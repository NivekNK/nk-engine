#include "nkpch.h"

#include "vulkan/buffer.h"

#include "vulkan/device.h"
#include "vulkan/command_buffer.h"

namespace nk {
    void Buffer::init(
        Device* device,
        VkAllocationCallbacks* vulkan_allocator,
        u64 size,
        VkBufferUsageFlags usage,
        u32 memory_property_flags,
        bool bind_on_create
    ) {
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;

        m_total_size = size;
        m_usage = usage;
        m_memory_property_flags = memory_property_flags;

        VkBufferCreateInfo buffer_create_info;
        memset(&buffer_create_info, 0, sizeof(buffer_create_info));
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = size;
        buffer_create_info.usage = usage;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // NOTE: Only used in one queue.

        VulkanCheck(vkCreateBuffer(m_device->get(), &buffer_create_info, m_vulkan_allocator, &m_buffer));

        // Gather memory requirements.
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(m_device->get(), m_buffer, &memory_requirements);
        if (!m_device->find_memory_index(memory_requirements.memoryTypeBits, memory_property_flags, &m_memory_index)) {
            ErrorLog("Unable to create vulkan Buffer because the required memory type index was not found.");
            return;
        }

        VkMemoryAllocateInfo memory_allocate_info;
        memset(&memory_allocate_info, 0, sizeof(memory_allocate_info));
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = m_memory_index;

        // Allocate memory.
        VkResult result = vkAllocateMemory(
            m_device->get(),
            &memory_allocate_info,
            m_vulkan_allocator,
            &m_memory);
        if (result != VK_SUCCESS) {
            ErrorLog("Unable to create vulkan Buffer because the memory allocation failed.");
            return;
        }

        if (bind_on_create)
            bind(0);
    }

    void Buffer::shutdown() {
        if (m_memory != nullptr) {
            vkFreeMemory(m_device->get(), m_memory, m_vulkan_allocator);
            m_memory = nullptr;
        }
        if (m_buffer != nullptr) {
            vkDestroyBuffer(m_device->get(), m_buffer, m_vulkan_allocator);
            m_buffer = nullptr;
        }
        m_total_size = 0;
        m_usage = 0;
        m_is_locked = false;
    }

    void Buffer::resize(u64 size, VkQueue queue, VkCommandPool pool) {
        VkBufferCreateInfo buffer_create_info;
        memset(&buffer_create_info, 0, sizeof(buffer_create_info));
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = size;
        buffer_create_info.usage = m_usage;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // NOTE: Only used in one queue.

        VkBuffer new_buffer;
        VulkanCheck(vkCreateBuffer(m_device->get(), &buffer_create_info, m_vulkan_allocator, &new_buffer));
    
        // Gather memory requirements.
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(m_device->get(), new_buffer, &memory_requirements);
    
        // Allocate memory info
        VkMemoryAllocateInfo memory_allocate_info;
        memset(&memory_allocate_info, 0, sizeof(memory_allocate_info));
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = m_memory_index;

        // Allocate the memory.
        VkDeviceMemory new_memory;
        VkResult result = vkAllocateMemory(
            m_device->get(),
            &memory_allocate_info,
            m_vulkan_allocator,
            &new_memory);
        if (result != VK_SUCCESS) {
            ErrorLog("Unable to create vulkan Buffer because the memory allocation failed.");
            return;
        }

        // Bind the memory.
        VulkanCheck(vkBindBufferMemory(m_device->get(), new_buffer, new_memory, 0));

        // Copy over the data
        copy_to({
            .pool = pool,
            .fence = nullptr,
            .queue = queue,
            .source = m_buffer,
            .source_offset = 0,
            .destination = new_buffer,
            .destination_offset = 0,
            .size = m_total_size,
        });

        // Make sure anything potentially using these is finished.
        vkDeviceWaitIdle(m_device->get());

        // Destroy the old buffer and memory.
        if (m_memory != nullptr) {
            vkFreeMemory(m_device->get(), m_memory, m_vulkan_allocator);
            m_memory = nullptr;
        }
        if (m_buffer != nullptr) {
            vkDestroyBuffer(m_device->get(), m_buffer, m_vulkan_allocator);
            m_buffer = nullptr;
        }

        // Set new properties.
        m_total_size = size;
        m_memory = new_memory;
        m_buffer = new_buffer;
    }

    void Buffer::bind(u64 offset) {
        VulkanCheck(vkBindBufferMemory(m_device->get(), m_buffer, m_memory, offset));
    }

    void* Buffer::lock_memory(u64 offset, u64 size, u32 flags) {
        void* data;
        VulkanCheck(vkMapMemory(m_device->get(), m_memory, offset, size, flags, &data));
        return data;
    }

    void Buffer::unlock_memory() {
        vkUnmapMemory(m_device->get(), m_memory);
    }

    void Buffer::load_data(u64 offset, u64 size, u32 flags, const void* data) {
        void* data_ptr;
        VulkanCheck(vkMapMemory(m_device->get(), m_memory, offset, size, flags, &data_ptr));
        memcpy(data_ptr, data, size);
        vkUnmapMemory(m_device->get(), m_memory);
    }

    void Buffer::copy_to(const BufferCopyInfo& copy_info) {
        vkQueueWaitIdle(copy_info.queue);

        // Create a one time use command buffer.
        CommandBuffer command_buffer;
        command_buffer.init(copy_info.pool, m_device, false, true);

        // Prepare the copy command and add it to the command buffer.
        VkBufferCopy copy_region;
        copy_region.srcOffset = copy_info.source_offset;
        copy_region.dstOffset = copy_info.destination_offset;
        copy_region.size = copy_info.size;

        vkCmdCopyBuffer(command_buffer, copy_info.source, copy_info.destination, 1, &copy_region);

        // Submit the buffer for execution and wait for it to complete.
        command_buffer.end_single_use(copy_info.queue);
    }
}
