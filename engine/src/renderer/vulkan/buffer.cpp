#include "nkpch.h"

#include "vulkan/buffer.h"

#include "vulkan/command_buffer.h"
#include "vulkan/device.h"
#include "vulkan/graphics_commands.h"

namespace nk {
    namespace {
        [[nodiscard]] renderer_error buffer_error(
            const renderer_error_code code,
            const VkResult native_code = VK_SUCCESS) noexcept {
            return {
                .code = code,
                .native_code = static_cast<i32>(native_code),
            };
        }

        [[nodiscard]] VkBufferUsageFlags vulkan_usage(
            const BufferUsage usage) noexcept {
            VkBufferUsageFlags flags = 0;
            if (has_usage(usage, BufferUsage::vertex))
                flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            if (has_usage(usage, BufferUsage::index))
                flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            if (has_usage(usage, BufferUsage::uniform))
                flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            if (has_usage(usage, BufferUsage::storage))
                flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            if (has_usage(usage, BufferUsage::transfer_source))
                flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            if (has_usage(usage, BufferUsage::transfer_destination))
                flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            return flags;
        }

        [[nodiscard]] VkMemoryPropertyFlags vulkan_memory(
            const MemoryUsage usage,
            const Device& device) noexcept {
            switch (usage) {
                case MemoryUsage::device_local:
                    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                case MemoryUsage::upload:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                        (device.supports_device_local_host_visible()
                            ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                            : 0);
                case MemoryUsage::readback:
                    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                        VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            }
            return 0;
        }

        [[nodiscard]] vk::AccessScope consumer_scope(
            const VkBufferUsageFlags usage) noexcept {
            vk::AccessScope scope{};
            if ((usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) != 0) {
                scope.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            }
            if ((usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0)
                scope.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            if ((usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0)
                scope.access |= VK_ACCESS_INDEX_READ_BIT;
            if ((usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0) {
                scope.stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                scope.access |= VK_ACCESS_UNIFORM_READ_BIT;
            }
            if ((usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0) {
                scope.stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                scope.access |= VK_ACCESS_SHADER_READ_BIT |
                    VK_ACCESS_SHADER_WRITE_BIT;
            }
            if ((usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0) {
                scope.stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
                scope.access |= VK_ACCESS_TRANSFER_READ_BIT;
            }
            if (scope.stages == 0)
                scope.stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            if (scope.access == 0)
                scope.access = VK_ACCESS_MEMORY_READ_BIT;
            return scope;
        }

        void destroy_buffer_storage(
            Device& device,
            VkAllocationCallbacks* allocator,
            const VkBuffer buffer,
            const VkDeviceMemory memory,
            const u64 memory_size,
            const u32 memory_index) noexcept {
            if (buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(device.get(), buffer, allocator);
            if (memory != VK_NULL_HANDLE)
                device.free_memory(memory, memory_size, memory_index);
        }
    }

    result<void, renderer_error> Buffer::init(
        Device* device,
        VkAllocationCallbacks* vulkan_allocator,
        const RenderBufferConfig& config,
        mem::Allocator* suballocation_metadata) noexcept {
        if (device == nullptr || device->get() == VK_NULL_HANDLE ||
            m_device != nullptr) {
            return err(buffer_error(
                renderer_error_code::initialization_failed));
        }
        m_device = device;
        m_vulkan_allocator = vulkan_allocator;
        auto initialized = init_render_buffer(config, suballocation_metadata);
        if (!initialized) {
            m_device = nullptr;
            m_vulkan_allocator = nullptr;
            return err(initialized.error());
        }
        return ok();
    }

    void Buffer::shutdown() noexcept {
        RenderBuffer::shutdown();
        m_device = nullptr;
        m_vulkan_allocator = nullptr;
    }

    result<void, renderer_error> Buffer::create_backend(
        const RenderBufferConfig& config) noexcept {
        m_total_size = config.size;
        m_usage = vulkan_usage(config.usage);
        m_memory_property_flags = vulkan_memory(config.memory, *m_device);
        m_persistently_mapped = config.persistent_map;
        if (m_usage == 0 || m_memory_property_flags == 0) {
            destroy_backend();
            return err(buffer_error(renderer_error_code::buffer_usage_invalid));
        }

        VkBufferCreateInfo buffer_create_info{};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = config.size;
        buffer_create_info.usage = m_usage;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkResult native_result = vkCreateBuffer(
            m_device->get(),
            &buffer_create_info,
            m_vulkan_allocator,
            &m_buffer);
        if (native_result != VK_SUCCESS) {
            destroy_backend();
            return err(buffer_error(
                renderer_error_code::buffer_creation_failed,
                native_result));
        }

        VkMemoryRequirements memory_requirements{};
        vkGetBufferMemoryRequirements(
            m_device->get(), m_buffer, &memory_requirements);
        if (!m_device->find_memory_index(
                memory_requirements.memoryTypeBits,
                m_memory_property_flags,
                &m_memory_index)) {
            // Device-local/cached/coherent host memory are preferences. The
            // generic contract only requires host visibility for upload and
            // readback; explicit flushes cover a non-coherent fallback.
            m_memory_property_flags &=
                ~(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                  VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            if (config.memory == MemoryUsage::device_local ||
                !m_device->find_memory_index(
                    memory_requirements.memoryTypeBits,
                    m_memory_property_flags,
                    &m_memory_index)) {
                m_memory_property_flags &=
                    ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                if (config.memory == MemoryUsage::device_local ||
                    !m_device->find_memory_index(
                        memory_requirements.memoryTypeBits,
                        m_memory_property_flags,
                        &m_memory_index)) {
                    destroy_backend();
                    return err(buffer_error(
                        renderer_error_code::buffer_memory_failed));
                }
            }
        }

        VkMemoryAllocateInfo memory_allocate_info{};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = m_memory_index;
        native_result = m_device->allocate_memory(
            memory_allocate_info, &m_memory);
        if (native_result != VK_SUCCESS) {
            destroy_backend();
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed,
                native_result));
        }
        m_memory_size = memory_requirements.size;

        native_result = vkBindBufferMemory(
            m_device->get(), m_buffer, m_memory, 0);
        if (native_result != VK_SUCCESS) {
            destroy_backend();
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed,
                native_result));
        }
        m_is_bound = true;

        if (m_persistently_mapped) {
            native_result = vkMapMemory(
                m_device->get(),
                m_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &m_mapped_data);
            if (native_result != VK_SUCCESS) {
                destroy_backend();
                return err(buffer_error(
                    renderer_error_code::buffer_map_failed,
                    native_result));
            }
        }
        return ok();
    }

    void Buffer::destroy_backend() noexcept {
        if (m_device != nullptr && m_device->get() != VK_NULL_HANDLE) {
            if (m_mapped_data != nullptr || m_is_locked)
                vkUnmapMemory(m_device->get(), m_memory);
            destroy_buffer_storage(
                *m_device,
                m_vulkan_allocator,
                m_buffer,
                m_memory,
                m_memory_size,
                m_memory_index);
        }
        m_total_size = 0;
        m_buffer = VK_NULL_HANDLE;
        m_usage = 0;
        m_is_locked = false;
        m_is_bound = false;
        m_persistently_mapped = false;
        m_mapped_data = nullptr;
        m_memory = VK_NULL_HANDLE;
        m_memory_size = 0;
        m_memory_index = 0;
        m_memory_property_flags = 0;
    }

    result<void*, renderer_error> Buffer::map_backend(
        const u64 offset,
        const u64) noexcept {
        if ((m_memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
            return err(buffer_error(renderer_error_code::buffer_map_failed));
        if (m_persistently_mapped)
            return ok(static_cast<void*>(
                static_cast<u8*>(m_mapped_data) + offset));
        if (m_is_locked)
            return err(buffer_error(renderer_error_code::buffer_map_failed));

        void* data = nullptr;
        const VkResult mapped = vkMapMemory(
            m_device->get(), m_memory, 0, VK_WHOLE_SIZE, 0, &data);
        if (mapped != VK_SUCCESS)
            return err(buffer_error(
                renderer_error_code::buffer_map_failed, mapped));
        m_is_locked = true;
        return ok(static_cast<void*>(static_cast<u8*>(data) + offset));
    }

    void Buffer::unmap_backend() noexcept {
        if (m_persistently_mapped || !m_is_locked)
            return;
        vkUnmapMemory(m_device->get(), m_memory);
        m_is_locked = false;
    }

    result<void, renderer_error> Buffer::flush_backend(
        const u64 offset,
        const u64 size) noexcept {
        if ((m_memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
            return err(buffer_error(renderer_error_code::buffer_map_failed));
        if ((m_memory_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0)
            return ok();

        const u64 atom = m_device->non_coherent_atom_size();
        const u64 aligned_offset = offset & ~(atom - 1);
        if (size > numeric::u64_max - offset)
            return err(buffer_error(renderer_error_code::buffer_range_invalid));
        const u64 end = offset + size;
        u64 aligned_end = end;
        const u64 remainder = end & (atom - 1);
        if (remainder != 0) {
            const u64 padding = atom - remainder;
            aligned_end = padding > numeric::u64_max - end
                ? m_memory_size
                : end + padding;
        }
        if (aligned_end > m_memory_size)
            aligned_end = m_memory_size;

        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = m_memory;
        range.offset = aligned_offset;
        range.size = aligned_end - aligned_offset;
        const VkResult flushed = vkFlushMappedMemoryRanges(
            m_device->get(), 1, &range);
        if (flushed != VK_SUCCESS)
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed, flushed));
        return ok();
    }

    result<void, renderer_error> Buffer::invalidate_backend(
        const u64 offset,
        const u64 size) noexcept {
        if ((m_memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
            return err(buffer_error(renderer_error_code::buffer_map_failed));
        if ((m_memory_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0)
            return ok();

        const u64 atom = m_device->non_coherent_atom_size();
        const u64 aligned_offset = offset & ~(atom - 1);
        if (size > numeric::u64_max - offset)
            return err(buffer_error(renderer_error_code::buffer_range_invalid));
        const u64 end = offset + size;
        const u64 remainder = end & (atom - 1);
        u64 aligned_end = end;
        if (remainder != 0) {
            const u64 padding = atom - remainder;
            aligned_end = padding > numeric::u64_max - end
                ? m_memory_size
                : end + padding;
        }
        if (aligned_end > m_memory_size)
            aligned_end = m_memory_size;

        const VkMappedMemoryRange range{
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = m_memory,
            .offset = aligned_offset,
            .size = aligned_end - aligned_offset,
        };
        const VkResult invalidated = vkInvalidateMappedMemoryRanges(
            m_device->get(), 1, &range);
        if (invalidated != VK_SUCCESS)
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed, invalidated));
        return ok();
    }

    result<void, renderer_error> Buffer::upload_backend(
        const u64 offset,
        const u64 size,
        const void* data) noexcept {
        if ((m_memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) {
            Buffer staging;
            auto initialized = staging.init(
                m_device,
                m_vulkan_allocator,
                {
                    .size = size,
                    .usage = BufferUsage::transfer_source,
                    .memory = MemoryUsage::upload,
                    .persistent_map = true,
                });
            if (!initialized)
                return err(initialized.error());
            auto staged = staging.upload(0, size, data);
            if (!staged)
                return err(staged.error());
            auto source = staging.view(0, size);
            auto target = view(offset, size);
            if (!source || !target)
                return err(buffer_error(renderer_error_code::buffer_range_invalid));
            return staging.copy_to(*source, *this, *target);
        }

        auto mapped = map_backend(offset, size);
        if (!mapped)
            return err(mapped.error());
        std::memcpy(*mapped, data, size);
        auto flushed = flush_backend(offset, size);
        if (!m_persistently_mapped)
            unmap_backend();
        if (!flushed)
            return err(flushed.error());
        return ok();
    }

    result<void, renderer_error> Buffer::copy_backend(
        const u64 source_offset,
        RenderBuffer& destination,
        const u64 destination_offset,
        const u64 size) noexcept {
        auto* target = dynamic_cast<Buffer*>(&destination);
        if (target == nullptr || target->m_device != m_device)
            return err(buffer_error(renderer_error_code::buffer_copy_failed));
        return copy_native({
            .pool = m_device->get_graphics_command_pool(),
            .queue = m_device->get_graphics_queue(),
            .source = m_buffer,
            .source_offset = source_offset,
            .destination = target->m_buffer,
            .destination_offset = destination_offset,
            .size = size,
            .destination_stages = consumer_scope(target->m_usage).stages,
            .destination_access = consumer_scope(target->m_usage).access,
        });
    }

    result<void, renderer_error> Buffer::resize_backend(
        const u64 size) noexcept {
        if (m_buffer == VK_NULL_HANDLE || m_memory == VK_NULL_HANDLE ||
            !m_is_bound || m_is_locked ||
            (m_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0 ||
            (m_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0) {
            return err(buffer_error(renderer_error_code::buffer_resize_failed));
        }

        VkBufferCreateInfo buffer_create_info{};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = size;
        buffer_create_info.usage = m_usage;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer new_buffer = VK_NULL_HANDLE;
        VkResult native_result = vkCreateBuffer(
            m_device->get(),
            &buffer_create_info,
            m_vulkan_allocator,
            &new_buffer);
        if (native_result != VK_SUCCESS)
            return err(buffer_error(
                renderer_error_code::buffer_creation_failed, native_result));

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(
            m_device->get(), new_buffer, &requirements);
        u32 new_memory_index = 0;
        if (!m_device->find_memory_index(
                requirements.memoryTypeBits,
                m_memory_property_flags,
                &new_memory_index)) {
            vkDestroyBuffer(m_device->get(), new_buffer, m_vulkan_allocator);
            return err(buffer_error(renderer_error_code::buffer_memory_failed));
        }

        VkMemoryAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = new_memory_index;
        VkDeviceMemory new_memory = VK_NULL_HANDLE;
        native_result = m_device->allocate_memory(allocation, &new_memory);
        if (native_result != VK_SUCCESS) {
            vkDestroyBuffer(m_device->get(), new_buffer, m_vulkan_allocator);
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed, native_result));
        }

        native_result = vkBindBufferMemory(
            m_device->get(), new_buffer, new_memory, 0);
        if (native_result != VK_SUCCESS) {
            destroy_buffer_storage(*m_device, m_vulkan_allocator,
                new_buffer, new_memory, requirements.size, new_memory_index);
            return err(buffer_error(
                renderer_error_code::buffer_memory_failed, native_result));
        }

        auto copied = copy_native({
            .pool = m_device->get_graphics_command_pool(),
            .queue = m_device->get_graphics_queue(),
            .source = m_buffer,
            .source_offset = 0,
            .destination = new_buffer,
            .destination_offset = 0,
            .size = m_total_size,
            .destination_stages = consumer_scope(m_usage).stages,
            .destination_access = consumer_scope(m_usage).access,
        });
        if (!copied) {
            destroy_buffer_storage(*m_device, m_vulkan_allocator,
                new_buffer, new_memory, requirements.size, new_memory_index);
            return err(copied.error());
        }

        void* new_mapped_data = nullptr;
        if (m_persistently_mapped) {
            native_result = vkMapMemory(m_device->get(), new_memory,
                0, VK_WHOLE_SIZE, 0, &new_mapped_data);
            if (native_result != VK_SUCCESS) {
                destroy_buffer_storage(*m_device, m_vulkan_allocator,
                    new_buffer, new_memory, requirements.size, new_memory_index);
                return err(buffer_error(
                    renderer_error_code::buffer_map_failed, native_result));
            }
        }

        if (m_mapped_data != nullptr)
            vkUnmapMemory(m_device->get(), m_memory);
        destroy_buffer_storage(*m_device, m_vulkan_allocator,
            m_buffer, m_memory, m_memory_size, m_memory_index);
        m_total_size = size;
        m_buffer = new_buffer;
        m_memory = new_memory;
        m_memory_size = requirements.size;
        m_memory_index = new_memory_index;
        m_mapped_data = new_mapped_data;
        return ok();
    }

    result<void, renderer_error> Buffer::copy_native(
        const BufferCopyInfo& copy_info) noexcept {
        if (copy_info.queue == VK_NULL_HANDLE ||
            copy_info.pool == VK_NULL_HANDLE || copy_info.size == 0)
            return err(buffer_error(renderer_error_code::buffer_copy_failed));

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
        vk::GraphicsCommands{*m_device, command_buffer}.buffer_barrier(
            {
                copy_info.destination,
                copy_info.destination_offset,
                copy_info.size,
            },
            {
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
            },
            {
                copy_info.destination_stages,
                copy_info.destination_access,
            });
        return command_buffer.end_single_use(copy_info.queue);
    }
}
