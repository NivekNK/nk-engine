#pragma once

#include <wy/wyhash.h>

#include "memory/allocator.h"

namespace nk {
    template <typename K, typename V>
    class Map {
    public:
        static constexpr u64 default_initial_capacity = 32;

        struct KeyValue {
            K key;
            V value;
        };

        class Value {
        public:
            Value() : m_value{std::nullopt} {}
            Value(V& v) : m_value{std::ref(v)} {}

            operator bool() { return m_value.has_value(); }
            operator V&() {
                if (!m_value.has_value()) {
                    return m_default_value;
                }
                return m_value.value().get();
            }

            V& value_or_default() {
                if (!m_value.has_value()) {
                    return m_default_value;
                }
                return m_value.value().get();
            }

            V& value_or(V value) {
                if (!m_value.has_value()) {
                    return value;
                }
                return m_value.value().get();
            }

        private:
            using OptRefValue = std::optional<std::reference_wrapper<V>>;
            OptRefValue m_value;

            V m_default_value;
        };

        struct Bucket {
            K key;
            V value;
            u64 distance;
            bool used;
        };

        Map()
            : m_allocator{nullptr},
              m_own_allocator{false},
              m_buckets{nullptr},
              m_capacity{0},
              m_used_capacity{0} {}

        // TODO: Implement
        ~Map() {
            if (m_allocator)
                m_allocator->free_lot(Bucket, m_buckets, m_capacity);
        }

        void init(Allocator* allocator, u64 initial_capacity = default_initial_capacity);
        void init_own(Allocator* allocator, u64 initial_capacity = default_initial_capacity) {} // TODO: Implement
        void init_list(Allocator* allocator, std::initializer_list<KeyValue> list) {}           // TODO: Implement
        void init_list_own(Allocator* allocator, std::initializer_list<KeyValue> list) {}       // TODO: Implement

        Map(const Map& map) {} // TODO: Implement
        Map(Map&& map) {}      // TODO: Implement

        Map& operator=(const Map& map) { return *this; } // TODO: Implement
        Map& operator=(Map&& map) { return *this; }      // TODO: Implement

        // BEGIN: Insert functions overloads
        template <typename Key>
            requires std::is_same_v<K, Key>
        void insert(const Key& key, const V& value);

        template <typename Key>
            requires std::is_same_v<K, Key> && std::is_arithmetic_v<K>
        void insert(const Key key, const V& value);

        template <typename Key>
            requires std::is_same_v<K, Key> && std::is_same_v<K, cstr>
        void insert(Key const& key, const V& value);

        template <size_t N>
            requires std::is_same_v<K, str> || std::is_same_v<K, vstr> || std::is_same_v<K, cstr> || std::is_same_v<K, const char (&)[N]>
        void insert(const char (&key)[N], const V& value);
        // END: Insert functions overloads

        // BEGIN: Get functions overloads
        template <typename Key>
            requires std::is_same_v<K, Key>
        Value get(const Key& key);

        template <typename Key>
            requires std::is_same_v<K, Key> && std::is_arithmetic_v<K>
        Value get(const Key key);

        template <typename Key>
            requires std::is_same_v<K, Key> && std::is_same_v<K, cstr>
        Value get(Key const& key);

        template <size_t N>
            requires std::is_same_v<K, str> || std::is_same_v<K, vstr> || std::is_same_v<K, cstr> || std::is_same_v<K, const char (&)[N]>
        Value get(const char (&key)[N]);
        // END: Get functions overloads

        // BEGIN: Remove functions overloads
        template <typename Key>
            requires std::is_same_v<K, Key>
        bool remove(const Key& key);

        template <typename Key>
            requires std::is_same_v<K, Key> && std::is_arithmetic_v<K>
        bool remove(const Key key);

        template <typename Key>
            requires std::is_same_v<K, Key> && std::is_same_v<K, cstr>
        bool remove(Key const& key);

        template <size_t N>
            requires std::is_same_v<K, str> || std::is_same_v<K, vstr> || std::is_same_v<K, cstr> || std::is_same_v<K, const char (&)[N]>
        bool remove(const char (&key)[N]);
        // END: Remove functions overloads

        Bucket* buckets() { return m_buckets; }
        u64 capacity() { return m_capacity; }

    private:
        Allocator* m_allocator;
        bool m_own_allocator;

        Bucket* m_buckets;
        u64 m_capacity;
        u64 m_used_capacity;
    };

    namespace detail {
        template <typename T>
        concept IsString = std::is_same_v<T, nk::str> || std::is_same_v<T, nk::vstr>;

        template <IsString T>
        inline bool equal(const T& lhs, const T& rhs) {
            return lhs == rhs;
        }

        template <typename T>
        concept IsCStr = std::is_same_v<T, nk::cstr> || std::is_same_v<T, char*>;

        template <IsCStr T>
        inline bool equal(T lhs, T rhs) {
            return strcmp(lhs, rhs) == 0;
        }

        template <IsCStr A, IsString B>
        inline bool equal(A a, B b) {
            return strcmp(a, b.c_str()) == 0;
        }

        template <typename T>
        concept IsNumeric = std::is_arithmetic_v<T>;

        template <IsNumeric T>
        inline bool equal(const T lhs, const T rhs) {
            return lhs == rhs;
        }

        template <typename T>
        inline u64 hash(const T& value, size_t seed = 0) {
            return wyhash(&value, sizeof(T), seed, _wyp);
        }

        template <>
        inline u64 hash<cstr>(cstr const& value, size_t seed) {
            return wyhash(value, strlen(value), seed, _wyp);
        }

        template <size_t N>
        inline u64 hash(const char (&value)[N], size_t seed = 0) {
            return wyhash(value, strlen(value), seed, _wyp);
        }
    }

    template <typename K, typename V>
    void Map<K, V>::init(Allocator* allocator, u64 initial_capacity) {
        m_allocator = allocator;
        m_own_allocator = false;

        if (initial_capacity < default_initial_capacity)
            initial_capacity = default_initial_capacity;

        m_buckets = m_allocator->allocate_lot(Bucket, initial_capacity);
        m_capacity = initial_capacity;
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key>
    void Map<K, V>::insert(const Key& key, const V& value) {
        Bucket current_bucket{
            .key = key,
            .value = value,
            .distance = 0,
            .used = true,
        };

        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;
        DebugLog("Initial Index: {} | Key: {}", initial_index, key);

        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                m_buckets[i] = current_bucket;
                return;
            }

            if (detail::equal(key, m_buckets[i].key)) {
                m_buckets[i].value = value;
                return;
            }

            if (current_bucket.distance > m_buckets[i].distance) {
                std::swap(m_buckets[i], current_bucket);
            }

            current_bucket.distance++;
        }
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key> && std::is_arithmetic_v<K>
    void Map<K, V>::insert(const Key key, const V& value) {
        Bucket current_bucket{
            .key = key,
            .value = value,
            .distance = 0,
            .used = true,
        };

        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;
        DebugLog("Initial Index: {} | Key: {}", initial_index, key);

        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                m_buckets[i] = current_bucket;
                return;
            }

            if (detail::equal(key, m_buckets[i].key)) {
                m_buckets[i].value = value;
                return;
            }

            if (current_bucket.distance > m_buckets[i].distance) {
                std::swap(m_buckets[i], current_bucket);
            }

            current_bucket.distance++;
        }
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key> && std::is_same_v<K, cstr>
    void Map<K, V>::insert(Key const& key, const V& value) {
        Bucket current_bucket{
            .key = key,
            .value = value,
            .distance = 0,
            .used = true,
        };

        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;
        DebugLog("Initial Index: {} | Key: {}", initial_index, key);

        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                m_buckets[i] = current_bucket;
                return;
            }

            if (detail::equal(key, m_buckets[i].key)) {
                m_buckets[i].value = value;
                return;
            }

            if (current_bucket.distance > m_buckets[i].distance) {
                std::swap(m_buckets[i], current_bucket);
            }

            current_bucket.distance++;
        }
    }

    template <typename K, typename V>
    template <size_t N>
        requires std::is_same_v<K, str> || std::is_same_v<K, vstr> || std::is_same_v<K, cstr> || std::is_same_v<K, const char (&)[N]>
    void Map<K, V>::insert(const char (&key)[N], const V& value) {
        Bucket current_bucket{
            .key = key,
            .value = value,
            .distance = 0,
            .used = true,
        };

        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;
        DebugLog("Initial Index: {} | Key: {}", initial_index, key);

        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                m_buckets[i] = current_bucket;
                return;
            }

            if (detail::equal(key, m_buckets[i].key)) {
                m_buckets[i].value = value;
                return;
            }

            if (current_bucket.distance > m_buckets[i].distance) {
                std::swap(m_buckets[i], current_bucket);
            }

            current_bucket.distance++;
        }
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key>
    Map<K, V>::Value Map<K, V>::get(const Key& key) {
        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;

        u64 current_distance = 0;
        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                return Value();
            }

            if (current_distance > m_buckets[i].distance) {
                return Value();
            }

            if (detail::equal(key, m_buckets[i].key)) {
                return Value(m_buckets[i].value);
            }

            current_distance++;
        }

        return Value();
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key> && std::is_arithmetic_v<K>
    Map<K, V>::Value Map<K, V>::get(const Key key) {
        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;

        u64 current_distance = 0;
        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                return Value();
            }

            if (current_distance > m_buckets[i].distance) {
                return Value();
            }

            if (detail::equal(key, m_buckets[i].key)) {
                return Value(m_buckets[i].value);
            }

            current_distance++;
        }

        return Value();
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key> && std::is_same_v<K, cstr>
    Map<K, V>::Value Map<K, V>::get(Key const& key) {
        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;

        u64 current_distance = 0;
        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                return Value();
            }

            if (current_distance > m_buckets[i].distance) {
                return Value();
            }

            if (detail::equal(key, m_buckets[i].key)) {
                return Value(m_buckets[i].value);
            }

            current_distance++;
        }

        return Value();
    }

    template <typename K, typename V>
    template <size_t N>
        requires std::is_same_v<K, str> || std::is_same_v<K, vstr> || std::is_same_v<K, cstr> || std::is_same_v<K, const char (&)[N]>
    Map<K, V>::Value Map<K, V>::get(const char (&key)[N]) {
        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;

        u64 current_distance = 0;
        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                return Value();
            }

            if (current_distance > m_buckets[i].distance) {
                return Value();
            }

            if (detail::equal(key, m_buckets[i].key)) {
                return Value(m_buckets[i].value);
            }

            current_distance++;
        }

        return Value();
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key>
    bool Map<K, V>::remove(const Key& key) {
        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;

        u64 current_distance = 0;
        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                return false;
            }

            if (current_distance > m_buckets[i].distance) {
                return false;
            }

            if (detail::equal(key, m_buckets[i].key)) {
                m_buckets[i] = Bucket{};
                return true;
            }

            current_distance++;
        }

        return false;
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key> && std::is_arithmetic_v<K>
    bool Map<K, V>::remove(const Key key) {
        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;

        u64 current_distance = 0;
        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                return false;
            }

            if (current_distance > m_buckets[i].distance) {
                return false;
            }

            if (detail::equal(key, m_buckets[i].key)) {
                m_buckets[i] = Bucket{};
                return true;
            }

            current_distance++;
        }

        return false;
    }

    template <typename K, typename V>
    template <typename Key>
        requires std::is_same_v<K, Key> && std::is_same_v<K, cstr>
    bool Map<K, V>::remove(Key const& key) {
        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;

        u64 current_distance = 0;
        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                return false;
            }

            if (current_distance > m_buckets[i].distance) {
                return false;
            }

            if (detail::equal(key, m_buckets[i].key)) {
                m_buckets[i] = Bucket{};
                return true;
            }

            current_distance++;
        }

        return false;
    }

    template <typename K, typename V>
    template <size_t N>
        requires std::is_same_v<K, str> || std::is_same_v<K, vstr> || std::is_same_v<K, cstr> || std::is_same_v<K, const char (&)[N]>
    bool Map<K, V>::remove(const char (&key)[N]) {
        const u64 hash = detail::hash(key);
        const u64 initial_index = hash % m_capacity;

        u64 current_distance = 0;
        for (u64 i = initial_index; i < m_capacity; i++) {
            if (!m_buckets[i].used) {
                return false;
            }

            if (current_distance > m_buckets[i].distance) {
                return false;
            }

            if (detail::equal(key, m_buckets[i].key)) {
                m_buckets[i] = Bucket{};
                return true;
            }

            current_distance++;
        }

        return false;
    }
}
