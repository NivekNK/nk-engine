#include "nkpch.h"

#include "vulkan/buffer.h"

#include "vulkan/command_buffer.h"
#include "vulkan/device.h"

namespace nk {
    namespace {
        renderer_error buffer_error(
            const renderer_error_code code,
            const VkResult native_code = VK_SUCCESS) noexcept {
            return {
                .code = code,
                .native_code = static_cast<i32>(native_code),
            };
        }

        void destroy_buffer_storage(
            Device& device,
            VkAllocationCallbacks* allocator,
            const VkBuffer buffer,
            const VkDeviceMemory memory) noexcept {
            if (buffer != nullptr)
                vkDestroyBuffer(device.get(), buffer, allocator);
            if (memory != nullptr)
                vkFreeMemory(device.get(), memory, allocator);
        }
    }

    result<void, renderer_error> Buffer::init(
        Device* device,
        VkAllocationCallbacks* vulkan_allocator,
        const u64 size,
        const VkBufferUsageFlags usage,
        const u32 memory_property_flags,
        const bool bind_on_create,
        mem::Allocator* suballocation_metadata,
        const u64 suballocation_capacity) {
        if (device == nullptr || device->get() == nullptr || size == 0 ||
            (suballocation_metadata == nullptr) !=
                (suballocation_capacity == 0) ||
            m_device != nullptr) {
            return err(buffer_error(
                renderer_error_code::initialization_failed));
        }

        if (suballocation_metadata != nullptr) {
            auto suballocator_initialized = m_suballocator.init(
                *suballocation_metadata,
                size,
                suballocation_capacity);
            if (!suballocator_initialized)
                return err(suballocator_initialized.error());
        }

        m_device = device;
        m_vulkan_allocator = vulkan_allocator;
        m_total_size = size;
        m_usage = usage;
        m_memory_property_flags = memory_property_flags;

        VkBufferCreateInfo buffer_create_info{};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = size;
        buffer_create_info.usage = usage;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult native_result = vkCreateBuffer(
            m_device->get(),
            &buffer_create_info,
            m_vulkan_allocator,
            &m_buffer);
        if (native_result != VK_SUCCESS) {
            const renderer_error error = buffer_error(
                renderer_error_code::buffer_creation_failed,
                native_result);
            shutdown();
            return err(error);
        }

        VkMemoryRequirements memory_requirements{};
        vkGetBufferMemoryRequirements(
            m_device->get(), m_buffer, &memory_requirements);
        if (!m_device->find_memory_index(
                memory_requirements.memoryTypeBits,
                memory_property_flags,
                &m_memory_index)) {
            shutdown();
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed));
        }

        VkMemoryAllocateInfo memory_allocate_info{};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = m_memory_index;

        native_result = vkAllocateMemory(
            m_device->get(),
            &memory_allocate_info,
            m_vulkan_allocator,
            &m_memory);
        if (native_result != VK_SUCCESS) {
            const renderer_error error = buffer_error(
                renderer_error_code::buffer_memory_failed,
                native_result);
            shutdown();
            return err(error);
        }

        if (bind_on_create) {
            native_result = vkBindBufferMemory(
                m_device->get(), m_buffer, m_memory, 0);
            if (native_result != VK_SUCCESS) {
                const renderer_error error = buffer_error(
                    renderer_error_code::buffer_memory_failed,
                    native_result);
                shutdown();
                return err(error);
            }
            m_is_bound = true;
        }

        return ok();
    }

    void Buffer::shutdown() {
        if (m_device != nullptr && m_device->get() != nullptr) {
            destroy_buffer_storage(
                *m_device,
                m_vulkan_allocator,
                m_buffer,
                m_memory);
        }

        m_suballocator.shutdown();
        m_device = nullptr;
        m_vulkan_allocator = nullptr;
        m_total_size = 0;
        m_buffer = nullptr;
        m_usage = 0;
        m_is_locked = false;
        m_is_bound = false;
        m_memory = nullptr;
        m_memory_index = 0;
        m_memory_property_flags = 0;
    }

    result<void, renderer_error> Buffer::resize(
        const u64 size,
        const VkQueue queue,
        const VkCommandPool pool) {
        if (m_device == nullptr || m_buffer == nullptr || m_memory == nullptr ||
            !m_is_bound || m_is_locked || size == 0) {
            return err(buffer_error(renderer_error_code::buffer_resize_failed));
        }
        if (size == m_total_size)
            return ok();

        VkBufferCreateInfo buffer_create_info{};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = size;
        buffer_create_info.usage = m_usage;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer new_buffer = nullptr;
        VkResult native_result = vkCreateBuffer(
            m_device->get(),
            &buffer_create_info,
            m_vulkan_allocator,
            &new_buffer);
        if (native_result != VK_SUCCESS) {
            return err(buffer_error(
                renderer_error_code::buffer_creation_failed,
                native_result));
        }

        VkMemoryRequirements memory_requirements{};
        vkGetBufferMemoryRequirements(
            m_device->get(), new_buffer, &memory_requirements);

        u32 new_memory_index = 0;
        if (!m_device->find_memory_index(
                memory_requirements.memoryTypeBits,
                m_memory_property_flags,
                &new_memory_index)) {
            vkDestroyBuffer(
                m_device->get(), new_buffer, m_vulkan_allocator);
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed));
        }

        VkMemoryAllocateInfo memory_allocate_info{};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = new_memory_index;

        VkDeviceMemory new_memory = nullptr;
        native_result = vkAllocateMemory(
            m_device->get(),
            &memory_allocate_info,
            m_vulkan_allocator,
            &new_memory);
        if (native_result != VK_SUCCESS) {
            vkDestroyBuffer(
                m_device->get(), new_buffer, m_vulkan_allocator);
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed,
                native_result));
        }

        native_result = vkBindBufferMemory(
            m_device->get(), new_buffer, new_memory, 0);
        if (native_result != VK_SUCCESS) {
            destroy_buffer_storage(
                *m_device, m_vulkan_allocator, new_buffer, new_memory);
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed,
                native_result));
        }

        const u64 copy_size = size < m_total_size ? size : m_total_size;
        auto copied = copy_to({
            .pool = pool,
            .fence = nullptr,
            .queue = queue,
            .source = m_buffer,
            .source_offset = 0,
            .destination = new_buffer,
            .destination_offset = 0,
            .size = copy_size,
        });
        if (!copied) {
            destroy_buffer_storage(
                *m_device, m_vulkan_allocator, new_buffer, new_memory);
            return err(copied.error());
        }

        native_result = vkDeviceWaitIdle(m_device->get());
        if (native_result != VK_SUCCESS) {
            destroy_buffer_storage(
                *m_device, m_vulkan_allocator, new_buffer, new_memory);
            return err(buffer_error(
                renderer_error_code::device_wait_failed,
                native_result));
        }

        if (m_suballocator.initialized()) {
            auto ranges_resized = m_suballocator.resize(size);
            if (!ranges_resized) {
                destroy_buffer_storage(
                    *m_device, m_vulkan_allocator, new_buffer, new_memory);
                return err(ranges_resized.error());
            }
        }

        destroy_buffer_storage(
            *m_device, m_vulkan_allocator, m_buffer, m_memory);
        m_total_size = size;
        m_buffer = new_buffer;
        m_memory = new_memory;
        m_memory_index = new_memory_index;
        return ok();
    }

    result<mem::MemoryRange, renderer_error> Buffer::reserve(
        const u64 size,
        const u64 alignment) noexcept {
        return m_suballocator.reserve(size, alignment);
    }

    result<void, renderer_error> Buffer::release(
        const mem::MemoryRange range) noexcept {
        return m_suballocator.release(range);
    }

    void Buffer::bind(const u64 offset) {
        VulkanCheck(vkBindBufferMemory(
            m_device->get(), m_buffer, m_memory, offset));
        m_is_bound = true;
    }

    void* Buffer::lock_memory(
        const u64 offset,
        const u64 size,
        const u32 flags) {
        void* data = nullptr;
        VulkanCheck(vkMapMemory(
            m_device->get(), m_memory, offset, size, flags, &data));
        m_is_locked = data != nullptr;
        return data;
    }

    void Buffer::unlock_memory() {
        if (!m_is_locked)
            return;
        vkUnmapMemory(m_device->get(), m_memory);
        m_is_locked = false;
    }

    result<void, renderer_error> Buffer::load_data(
        const u64 offset,
        const u64 size,
        const u32 flags,
        const void* data) {
        if (m_device == nullptr || m_memory == nullptr || data == nullptr ||
            size == 0 || offset > m_total_size ||
            size > m_total_size - offset) {
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed));
        }

        void* data_ptr = nullptr;
        const VkResult native_result = vkMapMemory(
            m_device->get(),
            m_memory,
            offset,
            size,
            flags,
            &data_ptr);
        if (native_result != VK_SUCCESS) {
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed,
                native_result));
        }

        std::memcpy(data_ptr, data, size);
        vkUnmapMemory(m_device->get(), m_memory);
        return ok();
    }

    result<void, renderer_error> Buffer::copy_to(
        const BufferCopyInfo& copy_info) {
        const VkResult wait_result = vkQueueWaitIdle(copy_info.queue);
        if (wait_result != VK_SUCCESS) {
            return err(buffer_error(
                renderer_error_code::device_wait_failed,
                wait_result));
        }

        CommandBuffer command_buffer;
        auto initialized = command_buffer.init(
            copy_info.pool, m_device, true, true);
        if (!initialized)
            return err(initialized.error());

        const VkBufferCopy copy_region{
            .srcOffset = copy_info.source_offset,
            .dstOffset = copy_info.destination_offset,
            .size = copy_info.size,
        };
        vkCmdCopyBuffer(
            command_buffer,
            copy_info.source,
            copy_info.destination,
            1,
            &copy_region);

        return command_buffer.end_single_use(copy_info.queue);
    }
}
