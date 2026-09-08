#include "nkpch.h"
#include "vulkan/vulkan_renderer.h"
#include "renderer/text_renderer.h"
#include "vulkan/graphics_commands.h"
#include "vulkan/resources/texture_data.h"
#include <glm/gtc/matrix_transform.hpp>

namespace nk {
    result<void, renderer_error> VulkanRenderer::prepare_text_frame(const TextFrame& text) {
        const renderer_error invalid{renderer_error_code::buffer_range_invalid, 0};
        if (m_render_pass_active || text.width != m_framebuffer_width ||
            text.height != m_framebuffer_height || text.vertices.length() > 65536u * 6 ||
            text.pages.length() > 64 || text.uploads.length() > 64)
            return err(invalid);
        for (const auto& batch : text.batches)
            if (batch.page >= text.pages.length() || !batch.vertex_count ||
                batch.first_vertex > text.vertices.length() ||
                batch.vertex_count > text.vertices.length() - batch.first_vertex ||
                batch.scissor.x > text.width || batch.scissor.y > text.height ||
                batch.scissor.width > text.width - batch.scissor.x ||
                batch.scissor.height > text.height - batch.scissor.y)
                return err(invalid);
        u64 upload_bytes = 0;
        for (const auto& upload : text.uploads) {
            const auto& page = upload.source;
            const auto& rect = page.dirty;
            if (!upload.texture || !upload.texture->valid() ||
                upload.texture->format != TextureFormat::r8_unorm ||
                page.width != upload.texture->width || page.height != upload.texture->height ||
                !page.width || !page.height || page.width > 4096 || page.height > 4096 ||
                page.pixels.length() != static_cast<u64>(page.width) * page.height ||
                !rect.width || !rect.height || rect.x >= page.width || rect.y >= page.height ||
                rect.width > page.width - rect.x || rect.height > page.height - rect.y)
                return err(renderer_error{renderer_error_code::texture_region_invalid, 0});
            upload_bytes = (upload_bytes + 3) & ~u64{3};
            upload_bytes += static_cast<u64>(rect.width) * rect.height;
        }
        // Fence for m_current_frame was retired by begin_frame(). If WSI changed
        // the slot count, swapchain recreation already drained all old work.
        if (m_text_frames.length() != m_in_flight_fences.length()) {
            if (m_text_frames.allocator()) (void)m_text_frames.arr_shutdown();
            if (!m_text_frames.arr_init(m_allocator, m_in_flight_fences.length()))
                return err(renderer_error{renderer_error_code::out_of_memory, 0});
        }
        auto& frame = m_text_frames[m_current_frame];
        auto ensure = [&](Buffer& buffer, u64 size, BufferUsage usage, u64 minimum)
            -> result<void, renderer_error> {
            if (!size || buffer.size() >= size) return ok();
            u64 capacity = minimum;
            while (capacity < size) capacity *= 2;
            // No outstanding references to this retired slot. Do not copy stale
            // contents during growth and never wait for unrelated frames here.
            buffer.shutdown();
            return buffer.init(&m_device, m_vulkan_allocator,
                {.size=capacity, .usage=usage, .memory=MemoryUsage::upload, .persistent_map=true});
        };
        const u64 vertex_bytes = text.vertices.length() * sizeof(TextVertex);
        auto allocated = ensure(frame.vertices, vertex_bytes, BufferUsage::vertex, 256 * 1024);
        if (!allocated) return err(allocated.error());
        if (vertex_bytes) {
            auto written = frame.vertices.upload(0, vertex_bytes, text.vertices.data());
            if (!written) return err(written.error());
        }
        allocated = ensure(frame.staging, upload_bytes, BufferUsage::transfer_source, 1024 * 1024);
        if (!allocated) return err(allocated.error());
        if (upload_bytes) {
            auto range = frame.staging.view(0, upload_bytes);
            if (!range) return err(range.error());
            auto mapped = frame.staging.map(*range);
            if (!mapped) return err(mapped.error());
            auto* bytes = static_cast<u8*>(mapped->data());
            u64 offset = 0;
            for (const auto& upload : text.uploads) {
                const auto& page = upload.source;
                const auto& rect = page.dirty;
                offset = (offset + 3) & ~u64{3};
                for (u32 row = 0; row < rect.height; ++row)
                    std::memcpy(bytes + offset + static_cast<u64>(row) * rect.width,
                        page.pixels.data() + static_cast<u64>(rect.y + row) * page.width + rect.x,
                        rect.width);
                offset += static_cast<u64>(rect.width) * rect.height;
            }
            auto flushed = mapped->flush();
            if (!flushed) return err(flushed.error());
        }
        // Queue submission makes flushed host writes available. Image hazards
        // include earlier frames on this same graphics queue, not just this CB.
        const auto commands = m_graphics_command_buffers[m_image_index].get();
        const vk::GraphicsCommands graphics{m_device, commands};
        const VkImageSubresourceRange whole{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        u64 offset = 0;
        for (const auto& upload : text.uploads) {
            const auto& rect = upload.source.dirty;
            auto* data = static_cast<TextureData*>(upload.texture->m_internal_data);
            offset = (offset + 3) & ~u64{3};
            graphics.transition(data->image.get(), whole, vk::ImageUse::sampled,
                vk::ImageUse::transfer_destination);
            VkBufferImageCopy copy{};
            copy.bufferOffset = offset;
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageOffset = {static_cast<i32>(rect.x), static_cast<i32>(rect.y), 0};
            copy.imageExtent = {rect.width, rect.height, 1};
            vkCmdCopyBufferToImage(commands, frame.staging.get(), data->image.get(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            graphics.transition(data->image.get(), whole, vk::ImageUse::transfer_destination,
                vk::ImageUse::sampled);
            offset += static_cast<u64>(rect.width) * rect.height;
        }
        return ok();
    }

    result<void, renderer_error> VulkanRenderer::draw_text_frame(const TextFrame& text) {
        if (text.batches.empty()) return ok();
        auto used = use_shader(text.shader);
        if (!used) return used;
        auto blended = set_material_blend_mode(MaterialBlendMode::transparent);
        if (!blended) return blended;
        auto bound = bind_shader_globals(text.shader);
        if (!bound) return bound;
        const glm::mat4 projection = glm::ortho(0.f, static_cast<f32>(text.width),
            static_cast<f32>(text.height), 0.f, -1.f, 1.f);
        auto set = set_shader_uniform(text.shader, text.projection_uniform, projection);
        if (!set) return set;
        auto applied = apply_shader_globals(text.shader);
        if (!applied) return applied;
        const auto commands = m_graphics_command_buffers[m_image_index].get();
        const vk::GraphicsCommands graphics{m_device, commands};
        const auto& buffer = m_text_frames[m_current_frame].vertices;
        for (const auto& batch : text.batches) {
            const auto& page = text.pages[batch.page];
            bound = bind_shader_instance(text.shader, page.instance);
            if (!bound) return bound;
            set = set_shader_sampler(text.shader, text.atlas_uniform, page.texture);
            if (!set) return set;
            applied = apply_shader_instance(text.shader, true);
            if (!applied) return applied;
            VkRect2D scissor{{static_cast<i32>(batch.scissor.x), static_cast<i32>(batch.scissor.y)},
                {batch.scissor.width, batch.scissor.height}};
            vkCmdSetScissor(commands, 0, 1, &scissor);
            graphics.draw({buffer.get(), static_cast<u64>(batch.first_vertex) * sizeof(TextVertex),
                static_cast<u64>(batch.vertex_count) * sizeof(TextVertex)}, batch.vertex_count);
        }
        const VkRect2D full{{0, 0}, {m_framebuffer_width, m_framebuffer_height}};
        vkCmdSetScissor(commands, 0, 1, &full);
        return ok();
    }
}
