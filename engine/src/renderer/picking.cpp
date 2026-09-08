#include "nkpch.h"

#include "renderer/picking.h"

#include "memory/allocator.h"

namespace nk {
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
