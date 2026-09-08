#include "nkpch.h"

#include "renderer/picking.h"

#include <algorithm>
#include <cmath>

#include "memory/allocator.h"

namespace nk {
    result<PickPosition, pick_error> physical_pick_position(
        const f32 logical_x,
        const f32 logical_y,
        const f32 content_scale,
        const u32 framebuffer_width,
        const u32 framebuffer_height) noexcept {
        if (!std::isfinite(logical_x) || !std::isfinite(logical_y) ||
            !std::isfinite(content_scale) || content_scale <= 0.0f ||
            framebuffer_width == 0 || framebuffer_height == 0) {
            return err(pick_error::invalid_position);
        }

        const f64 physical_x = static_cast<f64>(logical_x) * content_scale;
        const f64 physical_y = static_cast<f64>(logical_y) * content_scale;
        if (!std::isfinite(physical_x) || !std::isfinite(physical_y))
            return err(pick_error::invalid_position);

        const f64 maximum_x = static_cast<f64>(framebuffer_width - 1);
        const f64 maximum_y = static_cast<f64>(framebuffer_height - 1);
        const f64 clamped_x = std::clamp(physical_x, 0.0, maximum_x);
        const f64 clamped_y = std::clamp(physical_y, 0.0, maximum_y);
        return ok(PickPosition{
            static_cast<u32>(std::floor(clamped_x)),
            static_cast<u32>(std::floor(clamped_y)),
        });
    }

    result<u64, pick_error> PickQueue::submit(
        const PickPosition position,
        const u64 scene_revision,
        const PickRequestKind kind) noexcept {
        u64 sequence = m_next_sequence++;
        if (sequence == 0) {
            sequence = m_next_sequence++;
            if (sequence == 0)
                sequence = 1;
        }
        const PickRequest request{
            .sequence = sequence,
            .scene_revision = scene_revision,
            .position = position,
            .kind = kind,
        };
        m_current_scene_revision = scene_revision;
        if (kind == PickRequestKind::hover) {
            m_hover = request;
            m_hover_pending = true;
            return ok(sequence);
        }
        if (m_click_count == click_capacity)
            return err(pick_error::queue_full);
        const u32 tail = (m_click_head + m_click_count) % click_capacity;
        m_clicks[tail] = request;
        ++m_click_count;
        return ok(sequence);
    }

    bool PickQueue::consume(PickRequest& out_request) noexcept {
        const bool click_is_older = m_click_count != 0 &&
            (!m_hover_pending ||
             m_clicks[m_click_head].sequence < m_hover.sequence);
        if (click_is_older) {
            out_request = m_clicks[m_click_head];
            m_clicks[m_click_head] = {};
            m_click_head = (m_click_head + 1) % click_capacity;
            --m_click_count;
            return true;
        }
        if (!m_hover_pending)
            return false;
        out_request = m_hover;
        m_hover = {};
        m_hover_pending = false;
        return true;
    }

    void PickQueue::publish(PickResult result) noexcept {
        if (!result.request.valid() ||
            result.request.scene_revision != m_current_scene_revision)
            return;
        if (!m_result_pending ||
            result.request.sequence >= m_result.request.sequence) {
            m_result = result;
            m_result_pending = true;
        }
    }

    bool PickQueue::poll(PickResult& out_result) noexcept {
        if (!m_result_pending)
            return false;
        out_result = m_result;
        m_result = {};
        m_result_pending = false;
        return true;
    }

    void PickQueue::discard_scene(const u64 current_scene_revision) noexcept {
        m_current_scene_revision = current_scene_revision;
        if (m_hover_pending &&
            m_hover.scene_revision != current_scene_revision) {
            m_hover = {};
            m_hover_pending = false;
        }
        if (m_result_pending &&
            m_result.request.scene_revision != current_scene_revision) {
            m_result = {};
            m_result_pending = false;
        }
    }

    PickRegistry::~PickRegistry() {
        shutdown();
    }

    result<void, pick_error> PickRegistry::init(
        mem::Allocator& allocator,
        const u32 capacity) noexcept {
        if (m_initialized)
            return err(pick_error::already_initialized);
        if (capacity == 0 || capacity > maximum_capacity)
            return err(pick_error::invalid_capacity);
        if (!m_slots.arr_init(&allocator, capacity))
            return err(pick_error::out_of_memory);

        for (u32 index = 0; index < capacity; ++index) {
            m_slots[index].next_free = index + 1 < capacity
                ? static_cast<u16>(index + 1)
                : no_slot;
        }
        m_free_head = 0;
        m_count = 0;
        m_initialized = true;
        return ok();
    }

    void PickRegistry::shutdown() noexcept {
        if (m_slots.allocator() != nullptr)
            (void)m_slots.arr_shutdown();
        m_free_head = no_slot;
        m_count = 0;
        m_initialized = false;
    }

    result<PickId, pick_error> PickRegistry::acquire(
        const PickObject object) noexcept {
        if (!m_initialized)
            return err(pick_error::not_initialized);
        if (!object.valid())
            return err(pick_error::invalid_object);
        if (m_free_head == no_slot)
            return err(pick_error::capacity_exceeded);

        const u16 index = m_free_head;
        Slot& slot = m_slots[index];
        m_free_head = slot.next_free;
        slot.object = object;
        slot.next_free = no_slot;
        slot.occupied = true;
        ++m_count;
        return ok(make_id(index, slot.generation));
    }

    result<void, pick_error> PickRegistry::release(const PickId id) noexcept {
        if (!m_initialized)
            return err(pick_error::not_initialized);
        if (!id.valid())
            return err(pick_error::invalid_id);

        const u16 index = slot_index(id);
        if (index == no_slot || index >= m_slots.length())
            return err(pick_error::invalid_id);
        Slot& slot = m_slots[index];
        if (!slot.occupied || slot.generation != generation(id))
            return err(pick_error::stale_id);

        slot.object = {};
        slot.generation = next_generation(slot.generation);
        slot.next_free = m_free_head;
        slot.occupied = false;
        m_free_head = index;
        --m_count;
        return ok();
    }

    result<PickObject, pick_error> PickRegistry::resolve(
        const PickId id) const noexcept {
        if (!m_initialized)
            return err(pick_error::not_initialized);
        if (!id.valid())
            return err(pick_error::invalid_id);

        const u16 index = slot_index(id);
        if (index == no_slot || index >= m_slots.length())
            return err(pick_error::invalid_id);
        const Slot& slot = m_slots[index];
        if (!slot.occupied || slot.generation != generation(id))
            return err(pick_error::stale_id);
        return ok(slot.object);
    }

    PickId PickRegistry::make_id(
        const u16 slot,
        const u16 generation) noexcept {
        return PickId{
            (static_cast<u32>(generation) << 16) |
            (static_cast<u32>(slot) + 1),
        };
    }

    u16 PickRegistry::slot_index(const PickId id) noexcept {
        const u16 encoded = static_cast<u16>(id.value & 0xffffu);
        return encoded == 0 ? no_slot : static_cast<u16>(encoded - 1);
    }

    u16 PickRegistry::generation(const PickId id) noexcept {
        return static_cast<u16>(id.value >> 16);
    }

    u16 PickRegistry::next_generation(const u16 value) noexcept {
        const u16 next = static_cast<u16>(value + 1);
        return next == 0 ? 1 : next;
    }
}
