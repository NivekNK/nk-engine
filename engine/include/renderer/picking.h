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
        capacity_exceeded,
        invalid_id,
        stale_id,
        out_of_memory,
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
