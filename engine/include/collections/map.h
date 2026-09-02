#pragma once

#include <cstddef>
#include <concepts>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "core/hash.h"
#include "core/os.h"
#include "core/result.h"
#include "memory/allocator.h"
#include "memory/object_lifetime.h"

namespace nk::cl {
    enum class map_error : u8 {
        out_of_memory,
        capacity_overflow,
    };

    enum class insert_outcome : u8 {
        inserted,
        already_present,
        assigned,
    };

    namespace map_detail {
        template <typename Key>
        u64 hash_key(const Key& key, const u64 seed) noexcept {
            using nk::hash64;
            return hash64(key, seed);
        }

        template <typename StoredKey, typename Query>
        concept CompatibleKey = requires(
            const StoredKey& stored,
            const Query& query,
            const u64 seed) {
            { hash_key(query, seed) } -> std::same_as<u64>;
            { stored == query } -> std::convertible_to<bool>;
        };

        constexpr bool probe_distance_fits(const u64 distance) noexcept {
            return distance < numeric::u16_max;
        }

        constexpr u16 encode_probe_distance(const u64 distance) noexcept {
            return static_cast<u16>(distance + 1);
        }
    }

    template <mem::RelocatableObject K, mem::RelocatableObject V>
        requires std::equality_comparable<K> &&
                 std::is_move_constructible_v<K> &&
                 std::is_move_constructible_v<V>
    class map {
    private:
        struct Slot {
            Slot() noexcept {}

            alignas(K) std::byte key_storage[sizeof(K)];
            alignas(V) std::byte value_storage[sizeof(V)];

            K* key() noexcept {
                return std::launder(reinterpret_cast<K*>(key_storage));
            }
            const K* key() const noexcept {
                return std::launder(reinterpret_cast<const K*>(key_storage));
            }
            V* value() noexcept {
                return std::launder(reinterpret_cast<V*>(value_storage));
            }
            const V* value() const noexcept {
                return std::launder(reinterpret_cast<const V*>(value_storage));
            }
        };

        struct Layout {
            u64 distances_offset = 0;
            u64 slots_offset = 0;
            u64 size_bytes = 0;
        };

        struct Table {
            std::byte* storage = nullptr;
            u8* fingerprints = nullptr;
            u16* distances = nullptr;
            Slot* slots = nullptr;
            u64 capacity = 0;
            u64 size_bytes = 0;
        };

        struct Pending {
            template <typename KeyArg, typename ValueArg>
            Pending(
                const u64 initial_hash,
                KeyArg&& initial_key,
                ValueArg&& initial_value)
                : key{std::forward<KeyArg>(initial_key)},
                  value{std::forward<ValueArg>(initial_value)},
                  hash{initial_hash},
                  fingerprint{static_cast<u8>(initial_hash >> 56)} {}

            template <typename KeyArg, typename... ValueArgs>
            Pending(
                const u64 initial_hash,
                std::in_place_t,
                KeyArg&& initial_key,
                ValueArgs&&... value_args)
                : key{std::forward<KeyArg>(initial_key)},
                  value{std::forward<ValueArgs>(value_args)...},
                  hash{initial_hash},
                  fingerprint{static_cast<u8>(initial_hash >> 56)} {}

            K key;
            V value;
            u64 hash;
            u8 fingerprint;
            u64 distance = 0;
        };

    public:
        struct entry_ref {
            const K& key;
            V& value;
        };

        struct const_entry_ref {
            const K& key;
            const V& value;
        };

        template <bool Const>
        class basic_iterator {
        private:
            using Map = std::conditional_t<Const, const map, map>;

        public:
            basic_iterator() noexcept = default;

            auto operator*() const noexcept {
                Slot& slot = const_cast<Slot&>(m_owner->_slots()[m_index]);
                if constexpr (Const)
                    return const_entry_ref{*slot.key(), *slot.value()};
                else
                    return entry_ref{*slot.key(), *slot.value()};
            }

            basic_iterator& operator++() noexcept {
                ++m_index;
                _skip_empty();
                return *this;
            }

            bool operator==(const basic_iterator& other) const noexcept {
                return m_owner == other.m_owner && m_index == other.m_index;
            }

            bool operator!=(const basic_iterator& other) const noexcept {
                return !(*this == other);
            }

        private:
            basic_iterator(Map* owner, const u64 index) noexcept
                : m_owner{owner},
                  m_index{index} {
                _skip_empty();
            }

            void _skip_empty() noexcept {
                while (m_owner != nullptr &&
                       m_index < m_owner->m_capacity &&
                       !m_owner->_occupied(m_index)) {
                    ++m_index;
                }
            }

            Map* m_owner = nullptr;
            u64 m_index = 0;

            friend class map;
        };

        using iterator = basic_iterator<false>;
        using const_iterator = basic_iterator<true>;

        map() noexcept = default;

        map(const map&) = delete;
        map& operator=(const map&) = delete;

        map(map&& other) noexcept {
            _move_from(other);
        }

        map& operator=(map&& other) noexcept {
            if (this == &other)
                return *this;
            if (!_shutdown({__FILE__, __LINE__})) {
                _diagnostic("nk::cl::map move assignment could not release its destination.\n");
                return *this;
            }
            _move_from(other);
            return *this;
        }

        ~map() noexcept {
            if (!_shutdown({__FILE__, __LINE__}))
                _diagnostic("nk::cl::map destructor could not release its storage.\n");
        }

        [[nodiscard]] result<void, map_error> _map_init(
            mem::Allocator* allocator,
            const u64 expected_entries,
            const u64 seed) noexcept {
            return _initialize(
                {nullptr, 0},
                allocator,
                expected_entries,
                seed);
        }

#if NK_MEMORY_TRACKING_ENABLED
        [[nodiscard]] result<void, map_error> _map_init(
            const cstr file,
            const u32 line,
            mem::Allocator* allocator,
            const u64 expected_entries,
            const u64 seed) noexcept {
            return _initialize(
                {file, line},
                allocator,
                expected_entries,
                seed);
        }
#endif

        [[nodiscard]] result<void, map_error> reserve(
            const u64 expected_entries) noexcept {
            _require_initialized();
            u64 required_capacity = 0;
            if (!_capacity_for(expected_entries, required_capacity))
                return err(map_error::capacity_overflow);
            return _rehash({__FILE__, __LINE__}, required_capacity);
        }

        [[nodiscard]] result<insert_outcome, map_error> insert(
            const K& key,
            const V& value) noexcept
            requires std::is_copy_constructible_v<K> &&
                     std::is_copy_constructible_v<V> {
            return _insert({__FILE__, __LINE__}, key, value, false);
        }

        [[nodiscard]] result<insert_outcome, map_error> insert(
            K&& key,
            V&& value) noexcept {
            return _insert(
                {__FILE__, __LINE__},
                std::move(key),
                std::move(value),
                false);
        }

        [[nodiscard]] result<insert_outcome, map_error> insert_or_assign(
            const K& key,
            const V& value) noexcept
            requires std::is_copy_constructible_v<K> &&
                     std::is_copy_constructible_v<V> {
            return _insert({__FILE__, __LINE__}, key, value, true);
        }

        [[nodiscard]] result<insert_outcome, map_error> insert_or_assign(
            K&& key,
            V&& value) noexcept {
            return _insert(
                {__FILE__, __LINE__},
                std::move(key),
                std::move(value),
                true);
        }

        template <typename Query = K>
            requires map_detail::CompatibleKey<K, Query>
        V* find(const Query& key) noexcept {
            _require_initialized();
            const u64 index = _find_index(key);
            return index == npos ? nullptr : _slots()[index].value();
        }

        template <typename Query = K>
            requires map_detail::CompatibleKey<K, Query>
        const V* find(const Query& key) const noexcept {
            _require_initialized();
            const u64 index = _find_index(key);
            return index == npos ? nullptr : _slots()[index].value();
        }

        template <typename Query = K>
            requires map_detail::CompatibleKey<K, Query>
        bool contains(const Query& key) const noexcept {
            _require_initialized();
            return _find_index(key) != npos;
        }

        template <typename Query = K>
            requires map_detail::CompatibleKey<K, Query>
        V& at(const Query& key) noexcept {
            V* value = find(key);
            if (value == nullptr)
                _fatal("nk::cl::map::at could not find the requested key.\n");
            return *value;
        }

        template <typename Query = K>
            requires map_detail::CompatibleKey<K, Query>
        const V& at(const Query& key) const noexcept {
            const V* value = find(key);
            if (value == nullptr)
                _fatal("nk::cl::map::at could not find the requested key.\n");
            return *value;
        }

        template <typename Query = K>
            requires map_detail::CompatibleKey<K, Query>
        bool remove(const Query& key) noexcept {
            _require_initialized();
            const u64 index = _find_index(key);
            if (index == npos)
                return false;
            _erase_at(index);
            --m_length;
            return true;
        }

        void clear() noexcept {
            _require_initialized();
            _destroy_entries(_table());
            m_length = 0;
        }

        bool _map_shutdown() noexcept {
            return _shutdown({nullptr, 0});
        }

#if NK_MEMORY_TRACKING_ENABLED
        bool _map_shutdown(const cstr file, const u32 line) noexcept {
            return _shutdown({file, line});
        }
#endif

        u64 length() const noexcept { return m_length; }
        u64 capacity() const noexcept { return m_capacity; }
        u64 seed() const noexcept { return m_seed; }
        bool empty() const noexcept { return m_length == 0; }
        mem::Allocator* allocator() noexcept { return m_allocator; }
        const mem::Allocator* allocator() const noexcept { return m_allocator; }

        iterator begin() noexcept { return {this, 0}; }
        iterator end() noexcept { return {this, m_capacity}; }
        const_iterator begin() const noexcept { return {this, 0}; }
        const_iterator end() const noexcept { return {this, m_capacity}; }
        const_iterator cbegin() const noexcept { return {this, 0}; }
        const_iterator cend() const noexcept { return {this, m_capacity}; }

        // Any reserve/rehash invalidates every reference and iterator. Insert
        // without rehash preserves references, while remove invalidates the
        // erased entry and all entries shifted backward in its cluster.

        template <typename KeyArg, typename... ValueArgs>
            requires map_detail::CompatibleKey<K, std::remove_cvref_t<KeyArg>> &&
                     std::constructible_from<K, KeyArg&&> &&
                     std::constructible_from<V, ValueArgs&&...>
        [[nodiscard]] result<insert_outcome, map_error> try_emplace(
            KeyArg&& key,
            ValueArgs&&... value_args) noexcept {
            _require_initialized();
            const u64 hash = map_detail::hash_key(key, m_seed);
            if (_find_index(key, hash) != npos)
                return ok(insert_outcome::already_present);

            K owned_key{std::forward<KeyArg>(key)};
            auto capacity = _ensure_insert_capacity({__FILE__, __LINE__});
            if (!capacity)
                return err(capacity.error());

            Pending pending{
                hash,
                std::in_place,
                std::move(owned_key),
                std::forward<ValueArgs>(value_args)...};
            if (m_capacity > numeric::u16_max) {
                auto probe = _ensure_probe_capacity(
                    {__FILE__, __LINE__},
                    pending.hash);
                if (!probe)
                    return err(probe.error());
            }
            if (!_place_pending(_table(), pending))
                _fatal("nk::cl::map probe preflight diverged during insertion.\n");
            ++m_length;
            return ok(insert_outcome::inserted);
        }

    private:
        static constexpr u64 npos = numeric::u64_max;
        static constexpr u64 minimum_capacity = 8;

        static void _diagnostic(const cstr message) noexcept {
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
        }

        [[noreturn]] static void _fatal(const cstr message) noexcept {
            _diagnostic(message);
            std::abort();
        }

        static u64 _max_entries(const u64 capacity) noexcept {
            return capacity - capacity / 5;
        }

        static bool _capacity_for(
            const u64 expected_entries,
            u64& result) noexcept {
            if (expected_entries == 0) {
                result = 0;
                return true;
            }

            u64 capacity = minimum_capacity;
            while (_max_entries(capacity) < expected_entries) {
                if (capacity > numeric::u64_max / 2)
                    return false;
                capacity *= 2;
            }
            result = capacity;
            return true;
        }

        static constexpr u64 table_alignment =
            alignof(Slot) > alignof(u16) ? alignof(Slot) : alignof(u16);

        static bool _align_up(
            const u64 value,
            const u64 alignment,
            u64& aligned) noexcept {
            const u64 mask = alignment - 1;
            if (value > numeric::u64_max - mask)
                return false;
            aligned = (value + mask) & ~mask;
            return true;
        }

        static bool _layout_for(
            const u64 capacity,
            Layout& layout) noexcept {
            layout = {};
            if (capacity == 0)
                return true;
            if (capacity > numeric::u64_max / sizeof(u16))
                return false;

            if (!_align_up(capacity, alignof(u16), layout.distances_offset))
                return false;
            const u64 distance_bytes = capacity * sizeof(u16);
            if (layout.distances_offset > numeric::u64_max - distance_bytes)
                return false;
            const u64 metadata_end = layout.distances_offset + distance_bytes;
            if (!_align_up(metadata_end, alignof(Slot), layout.slots_offset))
                return false;
            if (capacity >
                (numeric::u64_max - layout.slots_offset) / sizeof(Slot)) {
                return false;
            }
            layout.size_bytes = layout.slots_offset + capacity * sizeof(Slot);
            return layout.size_bytes != 0;
        }

        static Table _table_from(
            std::byte* storage,
            const u64 capacity) noexcept {
            if (storage == nullptr)
                return {.capacity = capacity};
            Layout layout{};
            if (!_layout_for(capacity, layout))
                _fatal("nk::cl::map stored an invalid table layout.\n");
            return {
                .storage = storage,
                .fingerprints = reinterpret_cast<u8*>(storage),
                .distances = reinterpret_cast<u16*>(
                    storage + layout.distances_offset),
                .slots = reinterpret_cast<Slot*>(storage + layout.slots_offset),
                .capacity = capacity,
                .size_bytes = layout.size_bytes,
            };
        }

        static result<Table, map_error> _allocate_table(
            mem::Allocator& allocator,
            const mem::SourceLocation source,
            const u64 capacity) noexcept {
            if (capacity == 0)
                return ok(Table{});

            Layout layout{};
            if (!_layout_for(capacity, layout))
                return err(map_error::capacity_overflow);
#if NK_MEMORY_TRACKING_ENABLED
            void* raw_storage = allocator._allocate_raw(
                source.file,
                source.line,
                layout.size_bytes,
                table_alignment);
#else
            static_cast<void>(source);
            void* raw_storage = allocator._allocate_raw(
                layout.size_bytes,
                table_alignment);
#endif
            if (raw_storage == nullptr)
                return err(map_error::out_of_memory);

            Table table = _table_from(
                static_cast<std::byte*>(raw_storage),
                capacity);
            std::memset(
                table.storage,
                0,
                static_cast<std::size_t>(layout.slots_offset));
            for (u64 index = 0; index < capacity; ++index)
                std::construct_at(table.slots + index);
            return ok(table);
        }

        static bool _free_table(
            mem::Allocator& allocator,
            const mem::SourceLocation source,
            const Table table) noexcept {
            if (table.storage == nullptr)
                return table.capacity == 0;
            for (u64 index = table.capacity; index > 0; --index)
                std::destroy_at(table.slots + index - 1);
#if NK_MEMORY_TRACKING_ENABLED
            return allocator._free_raw(
                source.file,
                source.line,
                table.storage,
                table.size_bytes);
#else
            static_cast<void>(source);
            return allocator._free_raw(table.storage, table.size_bytes);
#endif
        }

        static bool _occupied(const Table& table, const u64 index) noexcept {
            return table.distances[index] != 0;
        }

        static u64 _distance(const Table& table, const u64 index) noexcept {
            return static_cast<u64>(table.distances[index] - 1);
        }

        static void _destroy_slot(Table& table, const u64 index) noexcept {
            if (!_occupied(table, index))
                return;
            Slot& slot = table.slots[index];
            std::destroy_at(slot.value());
            std::destroy_at(slot.key());
            table.distances[index] = 0;
            table.fingerprints[index] = 0;
        }

        static void _destroy_entries(Table table) noexcept {
            if (table.storage == nullptr)
                return;
            for (u64 index = 0; index < table.capacity; ++index)
                _destroy_slot(table, index);
        }

        static void _construct_slot(
            Table& table,
            const u64 index,
            Pending& pending) noexcept {
            Slot& slot = table.slots[index];
            std::construct_at(slot.key(), std::move(pending.key));
            std::construct_at(slot.value(), std::move(pending.value));
            table.fingerprints[index] = pending.fingerprint;
            table.distances[index] =
                map_detail::encode_probe_distance(pending.distance);
        }

        static void _swap_pending(
            Table& table,
            const u64 index,
            Pending& pending) noexcept {
            Slot& slot = table.slots[index];
            const u8 displaced_fingerprint = table.fingerprints[index];
            const u64 displaced_distance = _distance(table, index);
            K displaced_key{std::move(*slot.key())};
            V displaced_value{std::move(*slot.value())};
            _destroy_slot(table, index);

            _construct_slot(table, index, pending);

            std::destroy_at(&pending.value);
            std::destroy_at(&pending.key);
            std::construct_at(&pending.key, std::move(displaced_key));
            std::construct_at(&pending.value, std::move(displaced_value));
            pending.fingerprint = displaced_fingerprint;
            pending.distance = displaced_distance;
        }

        static bool _can_place_hash(
            const Table& table,
            const u64 hash) noexcept {
            const u64 mask = table.capacity - 1;
            u64 index = hash & mask;
            u64 distance = 0;
            for (u64 visited = 0; visited < table.capacity; ++visited) {
                if (!map_detail::probe_distance_fits(distance))
                    return false;
                if (!_occupied(table, index))
                    return true;
                const u64 stored_distance = _distance(table, index);
                if (stored_distance < distance)
                    distance = stored_distance;
                ++distance;
                index = (index + 1) & mask;
            }
            return false;
        }

        static bool _place_metadata(Table& table, const u64 hash) noexcept {
            const u64 mask = table.capacity - 1;
            u64 index = hash & mask;
            u64 distance = 0;
            u8 fingerprint = static_cast<u8>(hash >> 56);
            for (u64 visited = 0; visited < table.capacity; ++visited) {
                if (!map_detail::probe_distance_fits(distance))
                    return false;
                if (!_occupied(table, index)) {
                    table.fingerprints[index] = fingerprint;
                    table.distances[index] =
                        map_detail::encode_probe_distance(distance);
                    return true;
                }
                const u64 stored_distance = _distance(table, index);
                if (stored_distance < distance) {
                    const u8 displaced_fingerprint = table.fingerprints[index];
                    table.fingerprints[index] = fingerprint;
                    fingerprint = displaced_fingerprint;
                    table.distances[index] =
                        map_detail::encode_probe_distance(distance);
                    distance = stored_distance;
                }
                ++distance;
                index = (index + 1) & mask;
            }
            return false;
        }

        static bool _place_pending(Table table, Pending& pending) noexcept {
            const u64 mask = table.capacity - 1;
            u64 index = pending.hash & mask;
            if (table.capacity <= numeric::u16_max) {
                for (;;) {
                    if (!_occupied(table, index)) {
                        _construct_slot(table, index, pending);
                        return true;
                    }
                    if (_distance(table, index) < pending.distance)
                        _swap_pending(table, index, pending);
                    ++pending.distance;
                    index = (index + 1) & mask;
                }
            }

            for (u64 visited = 0; visited < table.capacity; ++visited) {
                if (!map_detail::probe_distance_fits(pending.distance))
                    return false;
                if (!_occupied(table, index)) {
                    _construct_slot(table, index, pending);
                    return true;
                }
                if (_distance(table, index) < pending.distance)
                    _swap_pending(table, index, pending);
                ++pending.distance;
                index = (index + 1) & mask;
            }
            return false;
        }

        [[nodiscard]] result<void, map_error> _initialize(
            const mem::SourceLocation source,
            mem::Allocator* allocator,
            const u64 expected_entries,
            const u64 seed) noexcept {
            if (m_allocator != nullptr || m_storage != nullptr ||
                allocator == nullptr || !allocator->is_initialized())
                _fatal("nk::cl::map initialization contract was violated.\n");

            u64 capacity = 0;
            if (!_capacity_for(expected_entries, capacity))
                return err(map_error::capacity_overflow);
            auto allocated = _allocate_table(*allocator, source, capacity);
            if (!allocated)
                return err(allocated.error());

            m_allocator = allocator;
            m_storage = allocated->storage;
            m_capacity = capacity;
            m_seed = seed;
            return ok();
        }

        [[nodiscard]] result<void, map_error> _ensure_insert_capacity(
            const mem::SourceLocation source) noexcept {
            if (m_length == numeric::u64_max)
                return err(map_error::capacity_overflow);
            if (m_capacity != 0 && m_length + 1 <= _max_entries(m_capacity))
                return ok();

            const u64 next_capacity = m_capacity == 0
                ? minimum_capacity
                : (m_capacity <= numeric::u64_max / 2
                    ? m_capacity * 2
                    : 0);
            if (next_capacity == 0)
                return err(map_error::capacity_overflow);
            return _rehash(source, next_capacity);
        }

        [[nodiscard]] result<void, map_error> _ensure_probe_capacity(
            const mem::SourceLocation source,
            const u64 hash) noexcept {
            if (_can_place_hash(_table(), hash))
                return ok();
            if (m_capacity > numeric::u64_max / 2)
                return err(map_error::capacity_overflow);

            auto grown = _rehash(source, m_capacity * 2);
            if (!grown)
                return grown;
            if (!_can_place_hash(_table(), hash))
                return err(map_error::capacity_overflow);
            return ok();
        }

        [[nodiscard]] result<void, map_error> _rehash(
            const mem::SourceLocation source,
            const u64 requested_capacity) noexcept {
            _require_initialized();
            if (requested_capacity <= m_capacity)
                return ok();

            auto allocated = _allocate_table(
                *m_allocator,
                source,
                requested_capacity);
            if (!allocated)
                return err(allocated.error());
            Table replacement = allocated.value();
            Table current = _table();

            if (requested_capacity > numeric::u16_max) {
                for (u64 index = 0; index < current.capacity; ++index) {
                    if (!_occupied(current, index))
                        continue;
                    const u64 hash = map_detail::hash_key(
                        *current.slots[index].key(),
                        m_seed);
                    if (!_place_metadata(replacement, hash)) {
                        if (!_free_table(*m_allocator, source, replacement)) {
                            _diagnostic(
                                "nk::cl::map could not release a rejected table.\n");
                        }
                        return err(map_error::capacity_overflow);
                    }
                }
                const auto metadata_bytes = static_cast<std::size_t>(
                    reinterpret_cast<std::byte*>(replacement.slots) -
                    replacement.storage);
                std::memset(replacement.storage, 0, metadata_bytes);
            }

            for (u64 index = 0; index < current.capacity; ++index) {
                if (!_occupied(current, index))
                    continue;
                Slot& source_slot = current.slots[index];
                const u64 hash = map_detail::hash_key(*source_slot.key(), m_seed);
                Pending pending{
                    hash,
                    std::move(*source_slot.key()),
                    std::move(*source_slot.value())};
                _destroy_slot(current, index);
                if (!_place_pending(replacement, pending))
                    _fatal("nk::cl::map metadata preflight diverged during rehash.\n");
            }

            m_storage = replacement.storage;
            m_capacity = requested_capacity;
            if (!_free_table(
                    *m_allocator,
                    source,
                    current)) {
                _diagnostic("nk::cl::map could not release storage after rehash.\n");
            }
            return ok();
        }

        template <typename KeyArg, typename ValueArg>
        [[nodiscard]] result<insert_outcome, map_error> _insert(
            const mem::SourceLocation source,
            KeyArg&& key,
            ValueArg&& value,
            const bool assign_existing) noexcept {
            _require_initialized();

            const u64 hash = map_detail::hash_key(key, m_seed);
            const u64 existing_index = _find_index(key, hash);
            if (existing_index != npos) {
                if (!assign_existing)
                    return ok(insert_outcome::already_present);

                V* destination = _slots()[existing_index].value();
                if (destination == std::addressof(value))
                    return ok(insert_outcome::assigned);
                V replacement{std::forward<ValueArg>(value)};
                std::destroy_at(destination);
                std::construct_at(destination, std::move(replacement));
                return ok(insert_outcome::assigned);
            }

            Pending pending{
                hash,
                std::forward<KeyArg>(key),
                std::forward<ValueArg>(value)};
            auto capacity = _ensure_insert_capacity(source);
            if (!capacity)
                return err(capacity.error());
            if (m_capacity > numeric::u16_max) {
                auto probe = _ensure_probe_capacity(source, pending.hash);
                if (!probe)
                    return err(probe.error());
            }
            if (!_place_pending(_table(), pending))
                _fatal("nk::cl::map probe preflight diverged during insertion.\n");
            ++m_length;
            return ok(insert_outcome::inserted);
        }

        template <typename Query>
            requires map_detail::CompatibleKey<K, Query>
        u64 _find_index(const Query& key) const noexcept {
            return _find_index(
                key,
                map_detail::hash_key(key, m_seed));
        }

        template <typename Query>
            requires map_detail::CompatibleKey<K, Query>
        u64 _find_index(const Query& key, const u64 hash) const noexcept {
            if (m_capacity == 0)
                return npos;

            const u64 mask = m_capacity - 1;
            u64 index = hash & mask;
            u64 distance = 0;
            const u8 fingerprint = static_cast<u8>(hash >> 56);
            const Table table = _table();
            while (distance < m_capacity) {
                if (!_occupied(table, index) ||
                    distance > _distance(table, index)) {
                    return npos;
                }
                if (table.fingerprints[index] == fingerprint &&
                    *table.slots[index].key() == key) {
                    return index;
                }
                ++distance;
                index = (index + 1) & mask;
            }
            return npos;
        }

        void _require_initialized() const noexcept {
            if (m_allocator == nullptr)
                _fatal("nk::cl::map operation requires initialization.\n");
        }

        void _erase_at(const u64 erased_index) noexcept {
            const u64 mask = m_capacity - 1;
            u64 current = erased_index;
            Table table = _table();
            _destroy_slot(table, current);

            for (;;) {
                const u64 next = (current + 1) & mask;
                if (!_occupied(table, next) || _distance(table, next) == 0)
                    return;

                Slot& next_slot = table.slots[next];
                Pending pending{
                    0,
                    std::move(*next_slot.key()),
                    std::move(*next_slot.value())};
                pending.fingerprint = table.fingerprints[next];
                pending.distance = _distance(table, next) - 1;
                _destroy_slot(table, next);
                _construct_slot(table, current, pending);
                current = next;
            }
        }

        bool _shutdown(const mem::SourceLocation source) noexcept {
            if (m_allocator == nullptr) {
                return m_storage == nullptr &&
                       m_capacity == 0 &&
                       m_length == 0;
            }

            Table table = _table();
            _destroy_entries(table);
            const bool released = _free_table(
                *m_allocator,
                source,
                table);
            if (!released && m_storage != nullptr)
                return false;

            m_allocator = nullptr;
            m_storage = nullptr;
            m_capacity = 0;
            m_length = 0;
            m_seed = 0;
            return true;
        }

        void _move_from(map& other) noexcept {
            m_allocator = other.m_allocator;
            m_storage = other.m_storage;
            m_capacity = other.m_capacity;
            m_length = other.m_length;
            m_seed = other.m_seed;

            other.m_allocator = nullptr;
            other.m_storage = nullptr;
            other.m_capacity = 0;
            other.m_length = 0;
            other.m_seed = 0;
        }

        Table _table() const noexcept {
            return _table_from(m_storage, m_capacity);
        }

        Slot* _slots() const noexcept {
            return _table().slots;
        }

        bool _occupied(const u64 index) const noexcept {
            return _occupied(_table(), index);
        }

        mem::Allocator* m_allocator = nullptr;
        std::byte* m_storage = nullptr;
        u64 m_capacity = 0;
        u64 m_length = 0;
        u64 m_seed = 0;
    };
}

#if NK_MEMORY_TRACKING_ENABLED
    #define map_init(allocator, expected_entries, seed) \
        _map_init(__FILE__, __LINE__, (allocator), (expected_entries), (seed))
    #define map_shutdown() \
        _map_shutdown(__FILE__, __LINE__)
#else
    #define map_init(allocator, expected_entries, seed) \
        _map_init((allocator), (expected_entries), (seed))
    #define map_shutdown() \
        _map_shutdown()
#endif
