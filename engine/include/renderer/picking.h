#pragma once

#include "collections/arr.h"
#include "core/result.h"

namespace nk {
    namespace mem { class Allocator; }

    enum class PickObjectKind : u8 {
        none,
        geometry,
        ui,
        text,
        custom,
    };

    struct PickObject {
        PickObjectKind kind = PickObjectKind::none;
        u64 owner = 0;

        [[nodiscard]] bool valid() const noexcept {
            return kind != PickObjectKind::none;
        }

        bool operator==(const PickObject&) const noexcept = default;
    };

    // Compact GPU-facing identity. Zero is always background/no selection.
    // The high 16 bits are a generation and the low 16 bits are slot + 1.
    struct PickId {
        u32 value = 0;

        [[nodiscard]] bool valid() const noexcept { return value != 0; }
        bool operator==(const PickId&) const noexcept = default;
    };

    enum class pick_error : u8 {
        already_initialized,
        not_initialized,
        invalid_capacity,
        invalid_object,
        invalid_position,
        capacity_exceeded,
        queue_full,
        invalid_id,
        stale_id,
        out_of_memory,
    };

    enum class PickRequestKind : u8 {
        hover,
        click,
    };

    struct PickPosition {
        u32 x = 0;
        u32 y = 0;

        bool operator==(const PickPosition&) const noexcept = default;
    };

    struct PickRequest {
        u64 sequence = 0;
        u64 scene_revision = 0;
        PickPosition position{};
        PickRequestKind kind = PickRequestKind::hover;

        [[nodiscard]] bool valid() const noexcept { return sequence != 0; }
        bool operator==(const PickRequest&) const noexcept = default;
    };

    struct PickResult {
        PickRequest request{};
        PickId id{};
        PickObject object{};
        u64 retired_frame = 0;

        [[nodiscard]] bool hit() const noexcept {
            return id.valid() && object.valid();
        }
    };

    // Input coordinates are logical and top-left based on both native
    // backends. Vulkan uses a negative-height viewport, so image texel Y has
    // the same orientation and does not need an additional flip.
    [[nodiscard]] result<PickPosition, pick_error> physical_pick_position(
        f32 logical_x,
        f32 logical_y,
        f32 content_scale,
        u32 framebuffer_width,
        u32 framebuffer_height) noexcept;

    // Allocation-free request coordinator. Hover work is coalesced while
    // clicks retain FIFO order so an input edge cannot be replaced by motion.
    class PickQueue final {
    public:
        static constexpr u32 click_capacity = 8;

        [[nodiscard]] result<u64, pick_error> submit(
            PickPosition position,
            u64 scene_revision,
            PickRequestKind kind) noexcept;
        [[nodiscard]] bool consume(PickRequest& out_request) noexcept;
        void publish(PickResult result) noexcept;
        [[nodiscard]] bool poll(PickResult& out_result) noexcept;
        void discard_scene(u64 current_scene_revision) noexcept;

        [[nodiscard]] u32 pending_click_count() const noexcept {
            return m_click_count;
        }
        [[nodiscard]] bool hover_pending() const noexcept {
            return m_hover_pending;
        }

    private:
        PickRequest m_clicks[click_capacity]{};
        PickRequest m_hover{};
        PickResult m_result{};
        u64 m_next_sequence = 1;
        u64 m_current_scene_revision = 0;
        u32 m_click_head = 0;
        u32 m_click_count = 0;
        bool m_hover_pending = false;
        bool m_result_pending = false;
    };

    class PickRegistry final {
    public:
        static constexpr u32 maximum_capacity = numeric::u16_max;

        PickRegistry() noexcept = default;
        ~PickRegistry();

        PickRegistry(const PickRegistry&) = delete;
        PickRegistry& operator=(const PickRegistry&) = delete;
        PickRegistry(PickRegistry&&) = delete;
        PickRegistry& operator=(PickRegistry&&) = delete;

        [[nodiscard]] result<void, pick_error> init(
            mem::Allocator& allocator,
            u32 capacity = 4096) noexcept;
        void shutdown() noexcept;

        [[nodiscard]] result<PickId, pick_error> acquire(
            PickObject object) noexcept;
        [[nodiscard]] result<void, pick_error> release(PickId id) noexcept;
        [[nodiscard]] result<PickObject, pick_error> resolve(
            PickId id) const noexcept;

        [[nodiscard]] u32 capacity() const noexcept {
            return static_cast<u32>(m_slots.length());
        }
        [[nodiscard]] u32 count() const noexcept { return m_count; }

    private:
        static constexpr u16 no_slot = numeric::u16_max;

        struct Slot {
            PickObject object{};
            u16 generation = 1;
            u16 next_free = no_slot;
            bool occupied = false;
        };

        [[nodiscard]] static PickId make_id(
            u16 slot,
            u16 generation) noexcept;
        [[nodiscard]] static u16 slot_index(PickId id) noexcept;
        [[nodiscard]] static u16 generation(PickId id) noexcept;
        [[nodiscard]] static u16 next_generation(u16 value) noexcept;

        cl::arr<Slot> m_slots;
        u16 m_free_head = no_slot;
        u32 m_count = 0;
        bool m_initialized = false;
    };
}
