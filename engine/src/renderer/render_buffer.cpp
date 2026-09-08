#include "nkpch.h"

#include "renderer/render_buffer.h"

namespace nk {
    namespace {
        [[nodiscard]] bool valid_alignment(const u64 alignment) noexcept {
            return alignment != 0 && (alignment & (alignment - 1)) == 0;
        }

        [[nodiscard]] bool range_fits(
            const u64 capacity,
            const u64 offset,
            const u64 size) noexcept {
            return size != 0 && offset <= capacity && size <= capacity - offset;
        }
    }

    MappedBufferRange::~MappedBufferRange() {
        reset();
    }

    MappedBufferRange::MappedBufferRange(MappedBufferRange&& other) noexcept
        : m_owner{other.m_owner}, m_view{other.m_view}, m_data{other.m_data} {
        other.m_owner = nullptr;
        other.m_view = {};
        other.m_data = nullptr;
    }

    MappedBufferRange& MappedBufferRange::operator=(
        MappedBufferRange&& other) noexcept {
        if (this == &other)
            return *this;
        reset();
        m_owner = other.m_owner;
        m_view = other.m_view;
        m_data = other.m_data;
        other.m_owner = nullptr;
        other.m_view = {};
        other.m_data = nullptr;
        return *this;
    }

    result<void, renderer_error> MappedBufferRange::flush() noexcept {
        if (m_owner == nullptr)
            return err(RenderBuffer::range_error());
        return m_owner->flush(*this);
    }

    void MappedBufferRange::reset() noexcept {
        if (m_owner != nullptr)
            (void)m_owner->unmap(*this);
        m_owner = nullptr;
        m_view = {};
        m_data = nullptr;
    }

    result<void, renderer_error> RenderBuffer::init_render_buffer(
        const RenderBufferConfig& config,
        mem::Allocator* metadata_allocator) noexcept {
        if (m_initialized || config.size == 0 ||
            config.usage == BufferUsage::none ||
            (config.persistent_map &&
             config.memory == MemoryUsage::device_local) ||
            ((config.suballocation_capacity == 0) !=
             (metadata_allocator == nullptr))) {
            return err(renderer_error{
                renderer_error_code::buffer_usage_invalid,
                0,
            });
        }

        auto created = create_backend(config);
        if (!created)
            return err(created.error());
        if (metadata_allocator != nullptr) {
            auto initialized = m_suballocator.init(
                *metadata_allocator,
                config.size,
                config.suballocation_capacity);
            if (!initialized) {
                destroy_backend();
                return err(initialized.error());
            }
        }

        m_config = config;
        ++m_generation;
        if (m_generation == numeric::invalid_id)
            ++m_generation;
        m_initialized = true;
        return ok();
    }

    result<RenderBufferView, renderer_error> RenderBuffer::reserve(
        const u64 size,
        const u64 alignment) noexcept {
        if (!m_initialized || !m_suballocator.initialized())
            return err(range_error());
        auto range = m_suballocator.reserve(size, alignment);
        if (!range)
            return err(range.error());
        return ok(RenderBufferView{
            .owner = this,
            .generation = m_generation,
            .offset = range->offset,
            .size = range->size,
        });
    }

    result<void, renderer_error> RenderBuffer::release(
        const RenderBufferView& range) noexcept {
        if (!valid(range) || !m_suballocator.initialized())
            return err(range_error());
        return m_suballocator.release({range.offset, range.size});
    }

    result<RenderBufferView, renderer_error> RenderBuffer::view(
        const u64 offset,
        const u64 size,
        const u64 alignment) const noexcept {
        if (!m_initialized || !valid_alignment(alignment) ||
            (offset & (alignment - 1)) != 0 ||
            !range_fits(m_config.size, offset, size)) {
            return err(range_error());
        }
        return ok(RenderBufferView{
            .owner = this,
            .generation = m_generation,
            .offset = offset,
            .size = size,
        });
    }

    bool RenderBuffer::valid(const RenderBufferView& range) const noexcept {
        return m_initialized && range.owner == this &&
            range.generation == m_generation &&
            range_fits(m_config.size, range.offset, range.size);
    }

    result<MappedBufferRange, renderer_error> RenderBuffer::map(
        const RenderBufferView& range) noexcept {
        if (!valid(range) || m_map_active ||
            m_config.memory == MemoryUsage::device_local) {
            return err(renderer_error{
                renderer_error_code::buffer_map_failed,
                0,
            });
        }
        auto mapped = map_backend(range.offset, range.size);
        if (!mapped)
            return err(mapped.error());
        if (*mapped == nullptr)
            return err(renderer_error{
                renderer_error_code::buffer_map_failed,
                0,
            });
        m_map_active = true;
        return ok(MappedBufferRange{*this, range, *mapped});
    }

    result<void, renderer_error> RenderBuffer::upload(
        const RenderBufferView& range,
        const void* data) noexcept {
        if (!valid(range) || data == nullptr)
            return err(range_error());
        return upload_backend(range.offset, range.size, data);
    }

    result<void, renderer_error> RenderBuffer::upload(
        const u64 offset,
        const u64 size,
        const void* data) noexcept {
        auto range = view(offset, size);
        if (!range)
            return err(range.error());
        return upload(*range, data);
    }

    result<void, renderer_error> RenderBuffer::copy_to(
        const RenderBufferView& source,
        RenderBuffer& destination,
        const RenderBufferView& target) noexcept {
        if (!valid(source) || !destination.valid(target) ||
            source.size != target.size ||
            !has_usage(m_config.usage, BufferUsage::transfer_source) ||
            !has_usage(destination.m_config.usage,
                BufferUsage::transfer_destination)) {
            return err(renderer_error{
                renderer_error_code::buffer_copy_failed,
                0,
            });
        }
        return copy_backend(
            source.offset,
            destination,
            target.offset,
            source.size);
    }

    result<void, renderer_error> RenderBuffer::resize(
        const u64 size) noexcept {
        if (!m_initialized || m_map_active || size <= m_config.size)
            return err(renderer_error{
                renderer_error_code::buffer_resize_failed,
                0,
            });

        const u64 previous_size = m_config.size;
        if (m_suballocator.initialized()) {
            auto metadata_resized = m_suballocator.resize(size);
            if (!metadata_resized)
                return err(metadata_resized.error());
        }

        auto resized = resize_backend(size);
        if (!resized) {
            if (m_suballocator.initialized()) {
                auto rolled_back = m_suballocator.resize(previous_size);
                Assert(rolled_back,
                    "RenderBuffer metadata rollback must be infallible.");
            }
            return err(resized.error());
        }
        m_config.size = size;
        return ok();
    }

    void RenderBuffer::shutdown() noexcept {
        if (!m_initialized)
            return;
        if (m_map_active) {
            unmap_backend();
            m_map_active = false;
        }
        destroy_backend();
        m_suballocator.shutdown();
        m_config = {};
        ++m_generation;
        if (m_generation == numeric::invalid_id)
            ++m_generation;
        m_initialized = false;
    }

    result<void, renderer_error> RenderBuffer::unmap(
        MappedBufferRange& mapping) noexcept {
        if (!m_map_active || mapping.m_owner != this ||
            !valid(mapping.m_view)) {
            return err(renderer_error{
                renderer_error_code::buffer_map_failed,
                0,
            });
        }
        unmap_backend();
        m_map_active = false;
        return ok();
    }

    result<void, renderer_error> RenderBuffer::flush(
        const MappedBufferRange& mapping) noexcept {
        if (!m_map_active || mapping.m_owner != this ||
            !valid(mapping.m_view)) {
            return err(renderer_error{
                renderer_error_code::buffer_map_failed,
                0,
            });
        }
        return flush_backend(mapping.m_view.offset, mapping.m_view.size);
    }

    renderer_error RenderBuffer::range_error() noexcept {
        return {renderer_error_code::buffer_range_invalid, 0};
    }
}
