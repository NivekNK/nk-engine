#include "nkpch.h"

#include "memory/free_list.h"

#include <limits>

namespace nk::mem {
    FreeList::~FreeList() {
        shutdown();
    }

    FreeList::FreeList(FreeList&& other) noexcept
        : m_ranges{std::move(other.m_ranges)} {
        move_from(other);
    }

    FreeList& FreeList::operator=(FreeList&& other) noexcept {
        if (this == &other)
            return *this;

        shutdown();
        m_ranges = std::move(other.m_ranges);
        move_from(other);
        return *this;
    }

    result<void, free_list_error> FreeList::init(
        Allocator& metadata_allocator,
        const u64 total_size,
        const u64 metadata_capacity) noexcept {
        if (m_initialized || m_ranges.allocator() != nullptr)
            return err(free_list_error::already_initialized);
        if (total_size == 0)
            return err(free_list_error::invalid_size);
        if (metadata_capacity == 0)
            return err(free_list_error::metadata_exhausted);

        constexpr u64 range_size = sizeof(MemoryRange);
        constexpr u64 maximum_allocation =
            static_cast<u64>(std::numeric_limits<std::size_t>::max());
        if (metadata_capacity > numeric::u64_max / range_size ||
            metadata_capacity > maximum_allocation / range_size) {
            return err(free_list_error::capacity_overflow);
        }

        if (!m_ranges.arr_init(&metadata_allocator, metadata_capacity))
            return err(free_list_error::out_of_memory);

        m_ranges[0] = {.offset = 0, .size = total_size};
        m_total_size = total_size;
        m_free_space = total_size;
        m_range_count = 1;
        m_initialized = true;
        return ok();
    }

    void FreeList::shutdown() noexcept {
        if (m_ranges.allocator() != nullptr)
            (void)m_ranges.arr_shutdown();

        m_total_size = 0;
        m_free_space = 0;
        m_range_count = 0;
        m_initialized = false;
    }

    result<MemoryRange, free_list_error> FreeList::reserve(
        const u64 size,
        const u64 alignment,
        const u64 alignment_bias) noexcept {
        if (!m_initialized)
            return err(free_list_error::not_initialized);
        if (size == 0)
            return err(free_list_error::invalid_size);
        if (!valid_alignment(alignment))
            return err(free_list_error::invalid_alignment);
        if (size > m_free_space)
            return err(free_list_error::out_of_space);

        bool metadata_blocked = false;
        const u64 alignment_mask = alignment - 1;
        const u64 normalized_bias = alignment_bias & alignment_mask;
        for (u64 index = 0; index < m_range_count; ++index) {
            MemoryRange& free = m_ranges[index];
            const u64 misalignment =
                (normalized_bias + (free.offset & alignment_mask)) &
                alignment_mask;
            const u64 padding = misalignment == 0
                ? 0
                : alignment - misalignment;
            if (padding > free.size || size > free.size - padding)
                continue;

            const u64 allocation_offset = free.offset + padding;
            const u64 consumed = padding + size;
            const u64 suffix_size = free.size - consumed;

            if (padding != 0 && suffix_size != 0 &&
                m_range_count == m_ranges.length()) {
                metadata_blocked = true;
                continue;
            }

            if (padding == 0) {
                if (suffix_size == 0) {
                    remove_range(index);
                } else {
                    free.offset = allocation_offset + size;
                    free.size = suffix_size;
                }
            } else {
                free.size = padding;
                if (suffix_size != 0) {
                    insert_range(index + 1, {
                        .offset = allocation_offset + size,
                        .size = suffix_size,
                    });
                }
            }

            m_free_space -= size;
            return ok(MemoryRange{
                .offset = allocation_offset,
                .size = size,
            });
        }

        return err(metadata_blocked
            ? free_list_error::metadata_exhausted
            : free_list_error::out_of_space);
    }

    result<void, free_list_error> FreeList::release(
        const MemoryRange range) noexcept {
        if (!m_initialized)
            return err(free_list_error::not_initialized);
        if (range.size == 0)
            return err(free_list_error::invalid_size);

        u64 released_end = 0;
        if (!range_end(range, released_end) || released_end > m_total_size)
            return err(free_list_error::invalid_range);

        u64 insertion = 0;
        while (insertion < m_range_count &&
               m_ranges[insertion].offset < range.offset) {
            ++insertion;
        }

        bool joins_previous = false;
        if (insertion != 0) {
            const MemoryRange& previous = m_ranges[insertion - 1];
            u64 previous_end = 0;
            if (!range_end(previous, previous_end))
                return err(free_list_error::invalid_range);
            if (previous_end > range.offset)
                return err(free_list_error::overlapping_range);
            joins_previous = previous_end == range.offset;
        }

        bool joins_next = false;
        if (insertion < m_range_count) {
            const MemoryRange& next = m_ranges[insertion];
            if (released_end > next.offset)
                return err(free_list_error::overlapping_range);
            joins_next = released_end == next.offset;
        }

        if (range.size > m_total_size - m_free_space)
            return err(free_list_error::invalid_range);
        if (!joins_previous && !joins_next &&
            m_range_count == m_ranges.length()) {
            return err(free_list_error::metadata_exhausted);
        }

        if (joins_previous && joins_next) {
            MemoryRange& previous = m_ranges[insertion - 1];
            const MemoryRange next = m_ranges[insertion];
            previous.size += range.size + next.size;
            remove_range(insertion);
        } else if (joins_previous) {
            m_ranges[insertion - 1].size += range.size;
        } else if (joins_next) {
            MemoryRange& next = m_ranges[insertion];
            next.offset = range.offset;
            next.size += range.size;
        } else {
            insert_range(insertion, range);
        }

        m_free_space += range.size;
        return ok();
    }

    result<void, free_list_error> FreeList::resize(
        const u64 new_size) noexcept {
        if (!m_initialized)
            return err(free_list_error::not_initialized);
        if (new_size == 0)
            return err(free_list_error::invalid_size);
        if (new_size == m_total_size)
            return ok();

        if (new_size > m_total_size) {
            const u64 growth = new_size - m_total_size;
            if (m_range_count != 0) {
                MemoryRange& tail = m_ranges[m_range_count - 1];
                u64 tail_end = 0;
                if (!range_end(tail, tail_end))
                    return err(free_list_error::invalid_range);
                if (tail_end == m_total_size) {
                    tail.size += growth;
                    m_total_size = new_size;
                    m_free_space += growth;
                    return ok();
                }
            }

            if (m_range_count == m_ranges.length())
                return err(free_list_error::metadata_exhausted);

            insert_range(m_range_count, {
                .offset = m_total_size,
                .size = growth,
            });
            m_total_size = new_size;
            m_free_space += growth;
            return ok();
        }

        if (m_range_count == 0)
            return err(free_list_error::shrink_blocked);

        MemoryRange& tail = m_ranges[m_range_count - 1];
        u64 tail_end = 0;
        if (!range_end(tail, tail_end) || tail_end != m_total_size ||
            tail.offset > new_size) {
            return err(free_list_error::shrink_blocked);
        }

        const u64 reduction = m_total_size - new_size;
        if (tail.offset == new_size)
            remove_range(m_range_count - 1);
        else
            tail.size -= reduction;

        m_total_size = new_size;
        m_free_space -= reduction;
        return ok();
    }

    result<void, free_list_error> FreeList::clear() noexcept {
        if (!m_initialized)
            return err(free_list_error::not_initialized);

        m_ranges[0] = {.offset = 0, .size = m_total_size};
        for (u64 index = 1; index < m_range_count; ++index)
            m_ranges[index] = {};
        m_range_count = 1;
        m_free_space = m_total_size;
        return ok();
    }

    const MemoryRange* FreeList::free_range(const u64 index) const noexcept {
        if (!m_initialized || index >= m_range_count)
            return nullptr;
        return &m_ranges[index];
    }

    bool FreeList::valid_alignment(const u64 alignment) noexcept {
        return alignment != 0 && (alignment & (alignment - 1)) == 0;
    }

    bool FreeList::range_end(
        const MemoryRange range,
        u64& end) noexcept {
        if (range.size > numeric::u64_max - range.offset)
            return false;
        end = range.offset + range.size;
        return true;
    }

    void FreeList::insert_range(
        const u64 index,
        const MemoryRange range) noexcept {
        for (u64 destination = m_range_count; destination > index; --destination)
            m_ranges[destination] = m_ranges[destination - 1];
        m_ranges[index] = range;
        ++m_range_count;
    }

    void FreeList::remove_range(const u64 index) noexcept {
        for (u64 source = index + 1; source < m_range_count; ++source)
            m_ranges[source - 1] = m_ranges[source];
        --m_range_count;
        m_ranges[m_range_count] = {};
    }

    void FreeList::move_from(FreeList& other) noexcept {
        m_total_size = other.m_total_size;
        m_free_space = other.m_free_space;
        m_range_count = other.m_range_count;
        m_initialized = other.m_initialized;

        other.m_total_size = 0;
        other.m_free_space = 0;
        other.m_range_count = 0;
        other.m_initialized = false;
    }
}
