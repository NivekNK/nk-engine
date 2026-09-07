#pragma once

#include "core/defines.h"
#include "vulkan/vk.h"

namespace nk {
    class Device;
}

namespace nk::vk {
    // A borrowed view into application-suballocated storage. The compatibility
    // backend uses VkBuffer + offset instead of requiring GPU-address commands.
    struct BufferView {
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };

    struct AccessScope {
        VkPipelineStageFlags stages;
        VkAccessFlags access;
    };

    enum class ImageUse {
        discard,
        transfer_destination,
        transfer_source,
        sampled,
        color_attachment,
        depth_attachment,
        present,
    };

    struct ImageAccess {
        VkImageLayout layout;
        AccessScope scope;
    };

    [[nodiscard]] constexpr ImageAccess image_access(ImageUse use) noexcept {
        switch (use) {
            case ImageUse::discard:
                return {VK_IMAGE_LAYOUT_UNDEFINED, {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0}};
            case ImageUse::transfer_destination:
                return {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT}};
            case ImageUse::transfer_source:
                return {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT}};
            case ImageUse::sampled:
                return {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_SHADER_READ_BIT}};
            case ImageUse::color_attachment:
                return {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT}};
            case ImageUse::depth_attachment:
                return {VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT}};
            case ImageUse::present:
                return {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0}};
        }
        return {};
    }

    struct RenderingTarget {
        VkImageView color;
        VkImageView depth;
        VkRect2D area;
        VkClearColorValue clear_color;
        bool clear;
    };

    // Small command surface inspired by NoGraphicsAPI: borrowed ranges, root
    // bytes, attachment descriptions and producer/consumer hazards. WSI/layout
    // details remain inside this backend; no newer GPU floor is imposed.
    class GraphicsCommands {
    public:
        GraphicsCommands(Device& device, VkCommandBuffer commands)
            : m_device{device}, m_commands{commands} {}

        void barrier(AccessScope before, AccessScope after) const;
        void transition(VkImage image, VkImageSubresourceRange range,
            ImageUse before, ImageUse after) const;
        void begin_rendering(const RenderingTarget& target) const;
        void end_rendering() const;
        void draw_indexed(BufferView vertices, BufferView indices, u32 index_count) const;
        void draw(BufferView vertices, u32 vertex_count) const;
        void push_root(VkPipelineLayout layout, VkShaderStageFlags stages,
            u32 offset, u32 size, const void* bytes) const;

    private:
        Device& m_device;
        VkCommandBuffer m_commands;
    };
}
