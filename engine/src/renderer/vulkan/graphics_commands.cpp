#include "nkpch.h"
#include "vulkan/graphics_commands.h"
#include "vulkan/device.h"

namespace nk::vk {
    void GraphicsCommands::barrier(const AccessScope before, const AccessScope after) const {
        if (m_device.synchronization2()) {
            VkMemoryBarrier2 memory{};
            memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            memory.srcStageMask = before.stages;
            memory.srcAccessMask = before.access;
            memory.dstStageMask = after.stages;
            memory.dstAccessMask = after.access;
            VkDependencyInfo dependency{};
            dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency.memoryBarrierCount = 1;
            dependency.pMemoryBarriers = &memory;
            m_device.commands().pipeline_barrier(m_commands, &dependency);
        } else {
            VkMemoryBarrier memory{};
            memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory.srcAccessMask = before.access;
            memory.dstAccessMask = after.access;
            vkCmdPipelineBarrier(m_commands, before.stages, after.stages,
                0, 1, &memory, 0, nullptr, 0, nullptr);
        }
    }

    void GraphicsCommands::transition(const VkImage image, const VkImageSubresourceRange range,
        const ImageUse before, const ImageUse after) const {
        ImageAccess source = image_access(before);
        const ImageAccess destination = image_access(after);
        // Discarding contents does not bypass an acquire semaphore. Its wait
        // and this layout transition must share an execution scope (e.g. color
        // output for a swapchain image), not TOP_OF_PIPE before the wait.
        // Any prior application use must already be retired by the caller.
        if (before == ImageUse::discard)
            source.scope.stages = destination.scope.stages;
        if (m_device.synchronization2()) {
            VkImageMemoryBarrier2 image_barrier{};
            image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            image_barrier.srcStageMask = source.scope.stages;
            image_barrier.srcAccessMask = source.scope.access;
            image_barrier.dstStageMask = destination.scope.stages;
            image_barrier.dstAccessMask = destination.scope.access;
            image_barrier.oldLayout = source.layout;
            image_barrier.newLayout = destination.layout;
            image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_barrier.image = image;
            image_barrier.subresourceRange = range;
            VkDependencyInfo dependency{};
            dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency.imageMemoryBarrierCount = 1;
            dependency.pImageMemoryBarriers = &image_barrier;
            m_device.commands().pipeline_barrier(m_commands, &dependency);
        } else {
            VkImageMemoryBarrier image_barrier{};
            image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_barrier.srcAccessMask = source.scope.access;
            image_barrier.dstAccessMask = destination.scope.access;
            image_barrier.oldLayout = source.layout;
            image_barrier.newLayout = destination.layout;
            image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image_barrier.image = image;
            image_barrier.subresourceRange = range;
            vkCmdPipelineBarrier(m_commands, source.scope.stages, destination.scope.stages,
                0, 0, nullptr, 0, nullptr, 1, &image_barrier);
        }
    }

    void GraphicsCommands::begin_rendering(const RenderingTarget& target) const {
        VkRenderingAttachmentInfo color{};
        color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color.imageView = target.color;
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = target.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color = target.clear_color;
        VkRenderingAttachmentInfo depth{};
        depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth.imageView = target.depth;
        depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.clearValue.depthStencil = {1.0f, 0};
        VkRenderingInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        info.renderArea = target.area;
        info.layerCount = 1;
        info.colorAttachmentCount = 1;
        info.pColorAttachments = &color;
        info.pDepthAttachment = target.depth ? &depth : nullptr;
        m_device.commands().begin_rendering(m_commands, &info);
    }

    void GraphicsCommands::end_rendering() const {
        m_device.commands().end_rendering(m_commands);
    }

    void GraphicsCommands::draw_indexed(const BufferView vertices,
        const BufferView indices, const u32 index_count) const {
        Assert(static_cast<u64>(index_count) * sizeof(u32) <= indices.size,
            "Indexed draw exceeds its borrowed buffer range.");
        vkCmdBindVertexBuffers(m_commands, 0, 1, &vertices.buffer, &vertices.offset);
        vkCmdBindIndexBuffer(m_commands, indices.buffer, indices.offset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(m_commands, index_count, 1, 0, 0, 0);
    }

    void GraphicsCommands::draw(const BufferView vertices, const u32 vertex_count) const {
        vkCmdBindVertexBuffers(m_commands, 0, 1, &vertices.buffer, &vertices.offset);
        vkCmdDraw(m_commands, vertex_count, 1, 0, 0);
    }

    void GraphicsCommands::push_root(const VkPipelineLayout layout,
        const VkShaderStageFlags stages, const u32 offset, const u32 size, const void* bytes) const {
        vkCmdPushConstants(m_commands, layout, stages, offset, size, bytes);
    }
}
