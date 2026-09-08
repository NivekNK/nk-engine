#include <cstring>
#include <type_traits>

#include <gtest/gtest.h>

#include "memory/malloc_allocator.h"
#include "renderer/render_buffer.h"

namespace {
    class FakeRenderBuffer final : public nk::RenderBuffer {
    public:
        ~FakeRenderBuffer() override { shutdown(); }

        nk::result<void, nk::renderer_error> init(
            const nk::RenderBufferConfig& config,
            nk::mem::Allocator* metadata = nullptr) {
            return init_render_buffer(config, metadata);
        }

        void fail_next_resize() noexcept { m_fail_resize = true; }
        nk::u32 unmap_count() const noexcept { return m_unmap_count; }
        const nk::u8* bytes() const noexcept { return m_bytes; }

    private:
        nk::result<void, nk::renderer_error> create_backend(
            const nk::RenderBufferConfig& config) noexcept override {
            if (config.size > sizeof(m_bytes))
                return nk::err(error(nk::renderer_error_code::out_of_memory));
            m_backend_size = config.size;
            std::memset(m_bytes, 0, sizeof(m_bytes));
            return nk::ok();
        }

        void destroy_backend() noexcept override {
            m_backend_size = 0;
            m_mapped = false;
        }

        nk::result<void*, nk::renderer_error> map_backend(
            const nk::u64 offset,
            nk::u64) noexcept override {
            if (m_mapped)
                return nk::err(error(nk::renderer_error_code::buffer_map_failed));
            m_mapped = true;
            return nk::ok(static_cast<void*>(m_bytes + offset));
        }

        void unmap_backend() noexcept override {
            if (m_mapped) {
                m_mapped = false;
                ++m_unmap_count;
            }
        }

        nk::result<void, nk::renderer_error> flush_backend(
            nk::u64,
            nk::u64) noexcept override {
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> invalidate_backend(
            nk::u64,
            nk::u64) noexcept override {
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> upload_backend(
            const nk::u64 offset,
            const nk::u64 size,
            const void* data) noexcept override {
            std::memcpy(m_bytes + offset, data, size);
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> copy_backend(
            const nk::u64 source_offset,
            nk::RenderBuffer& destination,
            const nk::u64 destination_offset,
            const nk::u64 size) noexcept override {
            auto* target = dynamic_cast<FakeRenderBuffer*>(&destination);
            if (target == nullptr)
                return nk::err(error(nk::renderer_error_code::buffer_copy_failed));
            std::memcpy(
                target->m_bytes + destination_offset,
                m_bytes + source_offset,
                size);
            return nk::ok();
        }

        nk::result<void, nk::renderer_error> resize_backend(
            const nk::u64 size) noexcept override {
            if (m_fail_resize) {
                m_fail_resize = false;
                return nk::err(error(nk::renderer_error_code::buffer_resize_failed));
            }
            if (size > sizeof(m_bytes))
                return nk::err(error(nk::renderer_error_code::out_of_memory));
            m_backend_size = size;
            return nk::ok();
        }

        static nk::renderer_error error(
            const nk::renderer_error_code code) noexcept {
            return {code, 0};
        }

        nk::u8 m_bytes[512]{};
        nk::u64 m_backend_size = 0;
        nk::u32 m_unmap_count = 0;
        bool m_mapped = false;
        bool m_fail_resize = false;
    };
}

static_assert(!std::is_copy_constructible_v<FakeRenderBuffer>);
static_assert(!std::is_copy_assignable_v<FakeRenderBuffer>);

TEST(RenderBuffer, ValidatesConfigurationRangesAndGeneration) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    FakeRenderBuffer buffer;
    EXPECT_FALSE(buffer.init({
        .size = 128,
        .usage = nk::BufferUsage::none,
        .memory = nk::MemoryUsage::device_local,
    }));
    ASSERT_TRUE(buffer.init({
        .size = 128,
        .usage = nk::BufferUsage::vertex |
            nk::BufferUsage::transfer_destination,
        .memory = nk::MemoryUsage::device_local,
        .suballocation_capacity = 8,
    }, &metadata));

    auto allocated = buffer.reserve(24, 16);
    ASSERT_TRUE(allocated);
    EXPECT_EQ(allocated->offset % 16, 0);
    EXPECT_TRUE(buffer.valid(*allocated));
    EXPECT_FALSE(buffer.view(120, 16));

    FakeRenderBuffer other;
    ASSERT_TRUE(other.init({
        .size = 128,
        .usage = nk::BufferUsage::vertex,
        .memory = nk::MemoryUsage::device_local,
    }));
    EXPECT_FALSE(other.release(*allocated));

    const nk::RenderBufferView stale = *allocated;
    ASSERT_TRUE(buffer.release(*allocated));
    buffer.shutdown();
    EXPECT_FALSE(buffer.valid(stale));
}

TEST(RenderBuffer, MapsWithScopeAndRejectsDoubleMap) {
    FakeRenderBuffer buffer;
    ASSERT_TRUE(buffer.init({
        .size = 64,
        .usage = nk::BufferUsage::uniform,
        .memory = nk::MemoryUsage::upload,
        .persistent_map = true,
    }));
    auto range = buffer.view(8, 16, 8);
    ASSERT_TRUE(range);

    {
        auto mapped = buffer.map(*range);
        ASSERT_TRUE(mapped);
        EXPECT_NE(mapped->data(), nullptr);
        EXPECT_FALSE(buffer.map(*range));
        std::memset(mapped->data(), 0x5a, mapped->size());
        EXPECT_TRUE(mapped->flush());
    }
    EXPECT_EQ(buffer.unmap_count(), 1);
}

TEST(RenderBuffer, CopiesExplicitRangesAndChecksUsage) {
    FakeRenderBuffer source;
    FakeRenderBuffer destination;
    ASSERT_TRUE(source.init({
        .size = 64,
        .usage = nk::BufferUsage::transfer_source,
        .memory = nk::MemoryUsage::upload,
    }));
    ASSERT_TRUE(destination.init({
        .size = 64,
        .usage = nk::BufferUsage::transfer_destination,
        .memory = nk::MemoryUsage::device_local,
    }));
    const nk::u8 payload[4]{1, 2, 3, 4};
    ASSERT_TRUE(source.upload(12, sizeof(payload), payload));
    auto source_range = source.view(12, sizeof(payload));
    auto target_range = destination.view(20, sizeof(payload));
    ASSERT_TRUE(source_range);
    ASSERT_TRUE(target_range);
    ASSERT_TRUE(source.copy_to(*source_range, destination, *target_range));
    EXPECT_EQ(std::memcmp(destination.bytes() + 20, payload, sizeof(payload)), 0);

    auto wrong_target = source.view(0, sizeof(payload));
    ASSERT_TRUE(wrong_target);
    EXPECT_FALSE(source.copy_to(*source_range, source, *wrong_target));
}

TEST(RenderBuffer, RollsBackMetadataWhenBackendResizeFails) {
    nk::mem::MallocAllocator metadata{nk::mem::untracked};
    FakeRenderBuffer buffer;
    ASSERT_TRUE(buffer.init({
        .size = 128,
        .usage = nk::BufferUsage::vertex |
            nk::BufferUsage::transfer_source |
            nk::BufferUsage::transfer_destination,
        .memory = nk::MemoryUsage::device_local,
        .suballocation_capacity = 4,
    }, &metadata));
    auto occupied = buffer.reserve(128, 1);
    ASSERT_TRUE(occupied);
    EXPECT_EQ(buffer.free_space(), 0);

    buffer.fail_next_resize();
    EXPECT_FALSE(buffer.resize(256));
    EXPECT_EQ(buffer.size(), 128);
    EXPECT_EQ(buffer.free_space(), 0);
    EXPECT_TRUE(buffer.valid(*occupied));

    ASSERT_TRUE(buffer.resize(256));
    EXPECT_EQ(buffer.size(), 256);
    EXPECT_EQ(buffer.free_space(), 128);
    EXPECT_TRUE(buffer.valid(*occupied));
}
