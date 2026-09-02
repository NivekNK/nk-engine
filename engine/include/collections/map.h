#pragma once

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

#include "core/hash.h"
#include "core/os.h"
#include "memory/allocator.h"
#include "memory/object_lifetime.h"

namespace nk::cl {
    namespace map_detail {
        template <typename Key>
        u64 hash_key(const Key& key, const u64 seed) noexcept {
            using nk::hash64;
            return hash64(key, seed);
        }
    }

    template <mem::RelocatableObject K, mem::RelocatableObject V>
        requires std::equality_comparable<K> &&
                 std::is_move_constructible_v<K> &&
                 std::is_move_constructible_v<V>
    class map {
    private:
        struct Bucket {
            u64 hash = 0;
            u64 distance = 0;
            bool occupied = false;
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

        struct Pending {
            template <typename KeyArg, typename ValueArg>
            Pending(
                const u64 initial_hash,
                KeyArg&& initial_key,
                ValueArg&& initial_value)
                : key{std::forward<KeyArg>(initial_key)},
                  value{std::forward<ValueArg>(initial_value)},
                  hash{initial_hash} {}

            K key;
            V value;
            u64 hash;
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
                Bucket& bucket = const_cast<Bucket&>(m_owner->m_buckets[m_index]);
                if constexpr (Const)
                    return const_entry_ref{*bucket.key(), *bucket.value()};
                else
                    return entry_ref{*bucket.key(), *bucket.value()};
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
                       !m_owner->m_buckets[m_index].occupied) {
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

        bool _map_init(
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
        bool _map_init(
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

        bool reserve(const u64 expected_entries) noexcept {
            u64 required_capacity = 0;
            if (!_capacity_for(expected_entries, required_capacity))
                return false;
            return _rehash({__FILE__, __LINE__}, required_capacity);
        }

        bool insert(const K& key, const V& value) noexcept
            requires std::is_copy_constructible_v<K> &&
                     std::is_copy_constructible_v<V> {
            return _insert({__FILE__, __LINE__}, key, value, false);
        }

        bool insert(K&& key, V&& value) noexcept {
            return _insert(
                {__FILE__, __LINE__},
                std::move(key),
                std::move(value),
                false);
        }

        bool insert_or_assign(const K& key, const V& value) noexcept
            requires std::is_copy_constructible_v<K> &&
                     std::is_copy_constructible_v<V> {
            return _insert({__FILE__, __LINE__}, key, value, true);
        }

        bool insert_or_assign(K&& key, V&& value) noexcept {
            return _insert(
                {__FILE__, __LINE__},
                std::move(key),
                std::move(value),
                true);
        }

        V* find(const K& key) noexcept {
            const u64 index = _find_index(key);
            return index == npos ? nullptr : m_buckets[index].value();
        }

        const V* find(const K& key) const noexcept {
            const u64 index = _find_index(key);
            return index == npos ? nullptr : m_buckets[index].value();
        }

        bool contains(const K& key) const noexcept {
            return _find_index(key) != npos;
        }

        V& at(const K& key) noexcept {
            V* value = find(key);
            if (value == nullptr)
                _fatal("nk::cl::map::at could not find the requested key.\n");
            return *value;
        }

        const V& at(const K& key) const noexcept {
            const V* value = find(key);
            if (value == nullptr)
                _fatal("nk::cl::map::at could not find the requested key.\n");
            return *value;
        }

        bool remove(const K& key) noexcept {
            const u64 index = _find_index(key);
            if (index == npos)
                return false;
            _erase_at(index);
            --m_length;
            return true;
        }

        void clear() noexcept {
            _destroy_entries(m_buckets, m_capacity);
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

        static Bucket* _allocate_buckets(
            mem::Allocator& allocator,
            const mem::SourceLocation source,
            const u64 capacity) noexcept {
            if (capacity == 0)
                return nullptr;
#if NK_MEMORY_TRACKING_ENABLED
            Bucket* buckets = allocator._allocate_lot_t<Bucket>(
                source.file,
                source.line,
                capacity);
#else
            static_cast<void>(source);
            Bucket* buckets = allocator._allocate_lot_t<Bucket>(capacity);
#endif
            if (buckets == nullptr)
                return nullptr;
            for (u64 index = 0; index < capacity; ++index)
                std::construct_at(buckets + index);
            return buckets;
        }

        static bool _free_buckets(
            mem::Allocator& allocator,
            const mem::SourceLocation source,
            Bucket* buckets,
            const u64 capacity) noexcept {
            if (buckets == nullptr)
                return capacity == 0;
            for (u64 index = capacity; index > 0; --index)
                std::destroy_at(buckets + index - 1);
#if NK_MEMORY_TRACKING_ENABLED
            return allocator._free_lot_t<Bucket>(
                source.file,
                source.line,
                buckets,
                capacity);
#else
            static_cast<void>(source);
            return allocator._free_lot_t<Bucket>(buckets, capacity);
#endif
        }

        static void _destroy_bucket(Bucket& bucket) noexcept {
            if (!bucket.occupied)
                return;
            std::destroy_at(bucket.value());
            std::destroy_at(bucket.key());
            bucket.occupied = false;
            bucket.hash = 0;
            bucket.distance = 0;
        }

        static void _destroy_entries(
            Bucket* buckets,
            const u64 capacity) noexcept {
            if (buckets == nullptr)
                return;
            for (u64 index = 0; index < capacity; ++index)
                _destroy_bucket(buckets[index]);
        }

        static void _construct_bucket(Bucket& bucket, Pending& pending) noexcept {
            std::construct_at(bucket.key(), std::move(pending.key));
            std::construct_at(bucket.value(), std::move(pending.value));
            bucket.hash = pending.hash;
            bucket.distance = pending.distance;
            bucket.occupied = true;
        }

        static void _swap_pending(Bucket& bucket, Pending& pending) noexcept {
            const u64 displaced_hash = bucket.hash;
            const u64 displaced_distance = bucket.distance;
            K displaced_key{std::move(*bucket.key())};
            V displaced_value{std::move(*bucket.value())};
            _destroy_bucket(bucket);

            _construct_bucket(bucket, pending);

            std::destroy_at(&pending.value);
            std::destroy_at(&pending.key);
            std::construct_at(&pending.key, std::move(displaced_key));
            std::construct_at(&pending.value, std::move(displaced_value));
            pending.hash = displaced_hash;
            pending.distance = displaced_distance;
        }

        static void _place_pending(
            Bucket* buckets,
            const u64 capacity,
            Pending& pending) noexcept {
            const u64 mask = capacity - 1;
            u64 index = pending.hash & mask;
            for (;;) {
                Bucket& bucket = buckets[index];
                if (!bucket.occupied) {
                    _construct_bucket(bucket, pending);
                    return;
                }
                if (bucket.distance < pending.distance)
                    _swap_pending(bucket, pending);
                ++pending.distance;
                index = (index + 1) & mask;
            }
        }

        bool _initialize(
            const mem::SourceLocation source,
            mem::Allocator* allocator,
            const u64 expected_entries,
            const u64 seed) noexcept {
            if (m_allocator != nullptr || m_buckets != nullptr ||
                allocator == nullptr || !allocator->is_initialized()) {
                return false;
            }

            u64 capacity = 0;
            if (!_capacity_for(expected_entries, capacity))
                return false;
            Bucket* buckets = _allocate_buckets(*allocator, source, capacity);
            if (capacity != 0 && buckets == nullptr)
                return false;

            m_allocator = allocator;
            m_buckets = buckets;
            m_capacity = capacity;
            m_seed = seed;
            return true;
        }

        bool _ensure_insert_capacity(
            const mem::SourceLocation source) noexcept {
            if (m_length == numeric::u64_max)
                return false;
            if (m_capacity != 0 && m_length + 1 <= _max_entries(m_capacity))
                return true;

            const u64 next_capacity = m_capacity == 0
                ? minimum_capacity
                : (m_capacity <= numeric::u64_max / 2
                    ? m_capacity * 2
                    : 0);
            if (next_capacity == 0)
                return false;
            return _rehash(source, next_capacity);
        }

        bool _rehash(
            const mem::SourceLocation source,
            const u64 requested_capacity) noexcept {
            if (requested_capacity <= m_capacity)
                return true;
            if (m_allocator == nullptr)
                return false;

            Bucket* replacement = _allocate_buckets(
                *m_allocator,
                source,
                requested_capacity);
            if (replacement == nullptr)
                return false;

            for (u64 index = 0; index < m_capacity; ++index) {
                Bucket& source_bucket = m_buckets[index];
                if (!source_bucket.occupied)
                    continue;
                Pending pending{
                    source_bucket.hash,
                    std::move(*source_bucket.key()),
                    std::move(*source_bucket.value())};
                _destroy_bucket(source_bucket);
                _place_pending(replacement, requested_capacity, pending);
            }

            Bucket* previous = m_buckets;
            const u64 previous_capacity = m_capacity;
            m_buckets = replacement;
            m_capacity = requested_capacity;
            if (!_free_buckets(
                    *m_allocator,
                    source,
                    previous,
                    previous_capacity)) {
                _diagnostic("nk::cl::map could not release buckets after rehash.\n");
            }
            return true;
        }

        template <typename KeyArg, typename ValueArg>
        bool _insert(
            const mem::SourceLocation source,
            KeyArg&& key,
            ValueArg&& value,
            const bool assign_existing) noexcept {
            if (m_allocator == nullptr)
                return false;

            const u64 hash = map_detail::hash_key(key, m_seed);
            const u64 existing_index = _find_index(key, hash);
            if (existing_index != npos) {
                if (!assign_existing)
                    return false;

                V* destination = m_buckets[existing_index].value();
                if (destination == std::addressof(value))
                    return true;
                V replacement{std::forward<ValueArg>(value)};
                std::destroy_at(destination);
                std::construct_at(destination, std::move(replacement));
                return true;
            }

            Pending pending{
                hash,
                std::forward<KeyArg>(key),
                std::forward<ValueArg>(value)};
            if (!_ensure_insert_capacity(source))
                return false;
            _place_pending(m_buckets, m_capacity, pending);
            ++m_length;
            return true;
        }

        u64 _find_index(const K& key) const noexcept {
            return _find_index(
                key,
                map_detail::hash_key(key, m_seed));
        }

        u64 _find_index(const K& key, const u64 hash) const noexcept {
            if (m_capacity == 0)
                return npos;

            const u64 mask = m_capacity - 1;
            u64 index = hash & mask;
            u64 distance = 0;
            while (distance < m_capacity) {
                const Bucket& bucket = m_buckets[index];
                if (!bucket.occupied || distance > bucket.distance)
                    return npos;
                if (bucket.hash == hash && *bucket.key() == key)
                    return index;
                ++distance;
                index = (index + 1) & mask;
            }
            return npos;
        }

        void _erase_at(const u64 erased_index) noexcept {
            const u64 mask = m_capacity - 1;
            u64 current = erased_index;
            _destroy_bucket(m_buckets[current]);

            for (;;) {
                const u64 next = (current + 1) & mask;
                Bucket& next_bucket = m_buckets[next];
                if (!next_bucket.occupied || next_bucket.distance == 0)
                    return;

                Pending pending{
                    next_bucket.hash,
                    std::move(*next_bucket.key()),
                    std::move(*next_bucket.value())};
                pending.distance = next_bucket.distance - 1;
                _destroy_bucket(next_bucket);
                _construct_bucket(m_buckets[current], pending);
                current = next;
            }
        }

        bool _shutdown(const mem::SourceLocation source) noexcept {
            if (m_allocator == nullptr) {
                return m_buckets == nullptr &&
                       m_capacity == 0 &&
                       m_length == 0;
            }

            _destroy_entries(m_buckets, m_capacity);
            const bool released = _free_buckets(
                *m_allocator,
                source,
                m_buckets,
                m_capacity);
            if (!released && m_buckets != nullptr)
                return false;

            m_allocator = nullptr;
            m_buckets = nullptr;
            m_capacity = 0;
            m_length = 0;
            m_seed = 0;
            return true;
        }

        void _move_from(map& other) noexcept {
            m_allocator = other.m_allocator;
            m_buckets = other.m_buckets;
            m_capacity = other.m_capacity;
            m_length = other.m_length;
            m_seed = other.m_seed;

            other.m_allocator = nullptr;
            other.m_buckets = nullptr;
            other.m_capacity = 0;
            other.m_length = 0;
            other.m_seed = 0;
        }

        mem::Allocator* m_allocator = nullptr;
        Bucket* m_buckets = nullptr;
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
