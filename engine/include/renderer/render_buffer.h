#pragma once

#include "core/result.h"
#include "renderer/buffer_suballocator.h"
#include "renderer/renderer_result.h"

namespace nk {
    enum class BufferUsage : u16 {
        none = 0,
        vertex = 1 << 0,
        index = 1 << 1,
        uniform = 1 << 2,
        storage = 1 << 3,
        transfer_source = 1 << 4,
        transfer_destination = 1 << 5,
    };

    [[nodiscard]] constexpr BufferUsage operator|(
        const BufferUsage left,
        const BufferUsage right) noexcept {
        return static_cast<BufferUsage>(
            static_cast<u16>(left) | static_cast<u16>(right));
    }

    [[nodiscard]] constexpr bool has_usage(
        const BufferUsage value,
        const BufferUsage expected) noexcept {
        return (static_cast<u16>(value) & static_cast<u16>(expected)) ==
            static_cast<u16>(expected);
    }

    enum class MemoryUsage : u8 {
        device_local,
        upload,
        readback,
    };

    struct RenderBufferConfig {
        u64 size = 0;
        BufferUsage usage = BufferUsage::none;
        MemoryUsage memory = MemoryUsage::device_local;
        bool persistent_map = false;
        u64 suballocation_capacity = 0;
    };

    class RenderBuffer;

    struct RenderBufferView {
        const RenderBuffer* owner = nullptr;
        u32 generation = numeric::invalid_id;
        u64 offset = 0;
        u64 size = 0;

        [[nodiscard]] explicit operator bool() const noexcept {
            return owner != nullptr && generation != numeric::invalid_id &&
                size != 0;
        }
    };

    class MappedBufferRange final {
    public:
        ~MappedBufferRange();

        MappedBufferRange(const MappedBufferRange&) = delete;
        MappedBufferRange& operator=(const MappedBufferRange&) = delete;
        MappedBufferRange(MappedBufferRange&& other) noexcept;
        MappedBufferRange& operator=(MappedBufferRange&& other) noexcept;

        [[nodiscard]] void* data() const noexcept { return m_data; }
        [[nodiscard]] u64 size() const noexcept { return m_view.size; }
        [[nodiscard]] const RenderBufferView& view() const noexcept {
            return m_view;
        }
        [[nodiscard]] result<void, renderer_error> flush() noexcept;
        [[nodiscard]] result<void, renderer_error> invalidate() noexcept;
        void reset() noexcept;

    private:
        MappedBufferRange(
            RenderBuffer& owner,
            RenderBufferView view,
            void* data) noexcept
            : m_owner{&owner}, m_view{view}, m_data{data} {}

        RenderBuffer* m_owner = nullptr;
        RenderBufferView m_view{};
        void* m_data = nullptr;

        friend class RenderBuffer;
    };

    /**
     * Renderer-facing buffer contract. API-specific handles and memory flags
     * live only in implementations such as the Vulkan Buffer.
     */
    class RenderBuffer {
    public:
        virtual ~RenderBuffer() = default;

        RenderBuffer(const RenderBuffer&) = delete;
        RenderBuffer& operator=(const RenderBuffer&) = delete;
        RenderBuffer(RenderBuffer&&) = delete;
        RenderBuffer& operator=(RenderBuffer&&) = delete;

        [[nodiscard]] result<RenderBufferView, renderer_error> reserve(
            u64 size,
            u64 alignment = 1) noexcept;
        [[nodiscard]] result<void, renderer_error> release(
            const RenderBufferView& view) noexcept;
        [[nodiscard]] result<RenderBufferView, renderer_error> view(
            u64 offset,
            u64 size,
            u64 alignment = 1) const noexcept;
        [[nodiscard]] bool valid(const RenderBufferView& view) const noexcept;

        [[nodiscard]] result<MappedBufferRange, renderer_error> map(
            const RenderBufferView& view) noexcept;
        [[nodiscard]] result<void, renderer_error> upload(
            const RenderBufferView& view,
            const void* data) noexcept;
        [[nodiscard]] result<void, renderer_error> upload(
            u64 offset,
            u64 size,
            const void* data) noexcept;
        [[nodiscard]] result<void, renderer_error> copy_to(
            const RenderBufferView& source,
            RenderBuffer& destination,
            const RenderBufferView& target) noexcept;
        [[nodiscard]] result<void, renderer_error> resize(u64 size) noexcept;
        void shutdown() noexcept;

        [[nodiscard]] bool initialized() const noexcept { return m_initialized; }
        [[nodiscard]] u64 size() const noexcept { return m_config.size; }
        [[nodiscard]] BufferUsage usage() const noexcept { return m_config.usage; }
        [[nodiscard]] MemoryUsage memory_usage() const noexcept {
            return m_config.memory;
        }
        [[nodiscard]] bool persistently_mapped() const noexcept {
            return m_config.persistent_map;
        }
        [[nodiscard]] bool supports_suballocation() const noexcept {
            return m_suballocator.initialized();
        }
        [[nodiscard]] u64 free_space() const noexcept {
            return m_suballocator.free_space();
        }
        [[nodiscard]] u64 occupied_space() const noexcept {
            return m_suballocator.occupied_space();
        }

    protected:
        RenderBuffer() noexcept = default;

        [[nodiscard]] result<void, renderer_error> init_render_buffer(
            const RenderBufferConfig& config,
            mem::Allocator* metadata_allocator) noexcept;

        [[nodiscard]] virtual result<void, renderer_error> create_backend(
            const RenderBufferConfig& config) noexcept = 0;
        virtual void destroy_backend() noexcept = 0;
        [[nodiscard]] virtual result<void*, renderer_error> map_backend(
            u64 offset,
            u64 size) noexcept = 0;
        virtual void unmap_backend() noexcept = 0;
        [[nodiscard]] virtual result<void, renderer_error> flush_backend(
            u64 offset,
            u64 size) noexcept = 0;
        [[nodiscard]] virtual result<void, renderer_error> invalidate_backend(
            u64 offset,
            u64 size) noexcept = 0;
        [[nodiscard]] virtual result<void, renderer_error> upload_backend(
            u64 offset,
            u64 size,
            const void* data) noexcept = 0;
        [[nodiscard]] virtual result<void, renderer_error> copy_backend(
            u64 source_offset,
            RenderBuffer& destination,
            u64 destination_offset,
            u64 size) noexcept = 0;
        [[nodiscard]] virtual result<void, renderer_error> resize_backend(
            u64 size) noexcept = 0;

    private:
        [[nodiscard]] result<void, renderer_error> unmap(
            MappedBufferRange& mapping) noexcept;
        [[nodiscard]] result<void, renderer_error> flush(
            const MappedBufferRange& mapping) noexcept;
        [[nodiscard]] result<void, renderer_error> invalidate(
            const MappedBufferRange& mapping) noexcept;
        [[nodiscard]] static renderer_error range_error() noexcept;

        RenderBufferConfig m_config{};
        BufferSuballocator m_suballocator;
        u32 m_generation = 0;
        bool m_initialized = false;
        bool m_map_active = false;

        friend class MappedBufferRange;
    };
}
