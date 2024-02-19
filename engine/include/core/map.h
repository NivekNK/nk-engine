#pragma once

#include "memory/allocator.h"
#include "wy/wy.hpp"
#include "core/result.h"

namespace nk {
    #define NK_MAP_INITIAL_CAPACITY 37
    #define NK_MAP_LOAD_FACTOR 0.5f

    enum class MapError {
        KeyNotFound,
    };

    template <typename K>
    concept IHash =
        (std::equality_comparable<K> || requires(K a, K b) {
            { strcmp(a, b) } -> std::convertible_to<bool>;
        }) &&
        requires(K k) {
            { wy::hash(k) } -> std::convertible_to<u64>;
        };

    template <IHash K, typename V>
    class Map {
    public:
        static_assert(!std::is_const_v<V>, "V must not be const");

        struct KeyValue {
            K key;
            V value;
            u32 jumps = 0;
            bool empty = true;
        };

        Map()
            : m_map{nullptr},
              m_allocator{nullptr} {}

        void init(Allocator* allocator) {
            m_allocator = allocator;
            m_capacity = 0;
            m_map = nullptr;
            grow();
        }

        void insert(const K& key, const V& value);

        Result<V&, MapError> get(const K& key);

        Result<KeyValue&, MapError> get_key_value(const K& key);

        void remove(const K& key);

    private:
        void grow();
        u64 get_index_from_hash(const K& key) const;
        void out_index_from_probing(u64& out_index, u32& out_jumps) const;

        bool index_is_empty(const u64 index) const { return m_map[index].empty; }

        template <typename T>
        bool key_is_equal(const T& a, const T& b) const {
            return a == b;
        }

        template <>
        bool key_is_equal<cstr>(const cstr& a, const cstr& b) const {
            return strcmp(a, b) == 0;
        }

        KeyValue* m_map;
        u64 m_capacity;
        u64 m_size;

        Allocator* m_allocator;
    };

    template <IHash K, typename V>
    void Map<K, V>::insert(const K& key, const V& value) {
        if (m_capacity * NK_MAP_LOAD_FACTOR <= m_size)
            grow();

        u64 index = get_index_from_hash(key);
        if (index_is_empty(index)) {
            m_map[index] = KeyValue {
                .key = key,
                .value = value,
                .jumps = 0,
                .empty = false,
            };
        } else if (key_is_equal(m_map[index].key, key)) {
            m_map[index].value = value;
        } else {
            u32 jumps = 0;
            out_index_from_probing(index, jumps);
            m_map[index] = KeyValue {
                .key = key,
                .value = value,
                .jumps = jumps,
                .empty = false,
            };
        }
    }

    template <IHash K, typename V>
    Result<V&, MapError> Map<K, V>::get(const K& key) {
        u64 index = get_index_from_hash(key);

        if (index_is_empty(index))
            return Err(MapError::KeyNotFound);

        if (!key_is_equal(m_map[index].key, key))
            out_index_from_probing(index);

        return Ok(m_map[index].value);
    }

    template <IHash K, typename V>
    Result<typename Map<K, V>::KeyValue&, MapError> Map<K, V>::get_key_value(const K& key) {
        u64 index = get_index_from_hash(key);

        if (index_is_empty(index))
            return Err(MapError::KeyNotFound);

        if (!key_is_equal(m_map[index].key, key))
            out_index_from_probing(index);

        return Ok(m_map[index]);
    }

    template <IHash K, typename V>
    void Map<K, V>::remove(const K& key) {
        u64 index = get_index_from_hash(key);

        if (index_is_empty(index))
            return;

        if (!key_is_equal(m_map[index].key, key))
            out_index_from_probing(index);

        m_map[index].empty = true;
    }

    template <IHash K, typename V>
    void Map<K, V>::grow() {
        const u64 capacity = m_capacity == 0 ? NK_MAP_INITIAL_CAPACITY : m_capacity * 2;

        KeyValue* map = m_allocator->allocate_lot(KeyValue*, capacity);
        if (m_capacity > 0) {
            memcpy(map, m_map, m_capacity * sizeof(KeyValue));
            m_allocator->free_lot(KeyValue, m_map, m_capacity);
        }

        m_map = map;
        m_capacity = capacity;
    }

    template <IHash K, typename V>
    u64 Map<K, V>::get_index_from_hash(const K& key) const {
        // Faster modulo: https://jguegant.github.io/blogs/tech/dense-hash-map.html
        return wy::hash(key) & (m_capacity - 1);
    }

    template <IHash K, typename V>
    void Map<K, V>::out_index_from_probing(u64& out_index, u32& out_jumps) const {
        while (!m_map[out_index].empty) {


            out_index++;
        }
    }
}
