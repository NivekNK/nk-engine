#pragma once

#include "memory/allocator.h"
#include "core/map/detail.h"

namespace nk {
    template <typename K, typename V>
    class Map {
    public:
        struct Iterator;

        static constexpr u64 iterator_end = u64_max;
        static constexpr u64 default_initial_capacity = 32;

        struct KeyValue {
            K key;
            V value;
        };

        Map()
            : m_allocator{nullptr},
              m_own_allocator{false},
              m_size{0},
              m_capacity{0},
              m_growth_left{0},
              m_control_bytes{detail::group_init_empty()},
              m_slots{nullptr} {
        }

        ~Map() {
            this->free();

            if (m_own_allocator)
                delete m_allocator;
        }

        Map(const Map&) = delete;
        Map& operator=(const Map&) = delete;

        Map(Map&& other) noexcept;
        Map& operator=(Map&& right) noexcept;

        void init(Allocator* allocator, const u64 initial_capacity = default_initial_capacity) {
            m_allocator = allocator;
            m_own_allocator = false;
            reserve(initial_capacity < default_initial_capacity ? default_initial_capacity : initial_capacity);
        }

        void init_own(Allocator* allocator, const u64 initial_capacity = default_initial_capacity) {
            m_allocator = allocator;
            m_own_allocator = true;
            reserve(initial_capacity < default_initial_capacity ? default_initial_capacity : initial_capacity);
        }

        void init_list(Allocator* allocator, std::initializer_list<KeyValue> list) {
            m_allocator = allocator;
            m_own_allocator = false;
            reserve(list.size() < default_initial_capacity ? default_initial_capacity : list.size());
            for (auto& item : list) {
                insert(item.key, item.value);
            }
        }

        void init_list_own(Allocator* allocator, std::initializer_list<KeyValue> list) {
            m_allocator = allocator;
            m_own_allocator = true;
            reserve(list.size() < default_initial_capacity ? default_initial_capacity : list.size());
            for (auto& item : list) {
                insert(item.key, item.value);
            }
        }

        Iterator find(const K& key);
        void insert(const K& key, const V& value);
        V& insert_use(const K& key);
        void insert_move(const K& key, V& value);
        bool remove(const K& key);
        bool remove(const Iterator& it);

        using OptRefValue = std::optional<std::reference_wrapper<V>>;
        OptRefValue get(const K& key);
        OptRefValue get(const Iterator& it);

        OptRefValue operator[](const K& key);

        using OptRefKeyValue = std::optional<std::reference_wrapper<KeyValue>>;
        OptRefKeyValue get_key_value(const K& key);
        OptRefKeyValue get_key_value(const Iterator& it);

        void clear();
        void free();

        void reserve(u64 new_size);

        u64 size() const { return m_size; }

        struct Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = KeyValue;
            using pointer = KeyValue*;
            using reference = KeyValue&;

            Iterator(KeyValue* slots, u64 index, i8* m_control_bytes)
                : m_slots{slots}, m_index{index}, m_control_bytes{m_control_bytes} {}

            reference operator*() const { return m_slots[m_index]; }
            pointer operator->() { return &m_slots[m_index]; }

            Iterator& operator++() {
                m_index++;
                skip_empty_or_deleted();
                return *this;
            }

            Iterator operator++(i32) {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const Iterator& a, const Iterator& b) {
                return a.m_index == b.m_index;
            }

            friend bool operator!=(const Iterator& a, const Iterator& b) {
                return a.m_index != b.m_index;
            }

            void skip_empty_or_deleted() {
                i8* ctrl = m_control_bytes + m_index;

                while (detail::control_is_empty_or_deleted(*ctrl)) {
                    u32 shift = detail::GroupSse2Impl{ctrl}.count_leading_empty_or_deleted();
                    ctrl += shift;
                    m_index += shift;
                }

                if (*ctrl == detail::control_bitmask_sentinel)
                    m_index = iterator_end;
            }

            u64 index() const { return m_index; }

        private:
            i8* m_control_bytes;
            KeyValue* m_slots;

            u64 m_index;
        };

        Iterator begin() {
            auto it = Iterator(m_slots, 0, m_control_bytes);
            it.skip_empty_or_deleted();
            return it;
        }
        Iterator end() { return Iterator(m_slots, iterator_end, m_control_bytes); }

        Iterator begin() const {
            auto it = Iterator(m_slots, 0, m_control_bytes);
            it.skip_empty_or_deleted();
            return it;
        }

        Iterator end() const { return Iterator(m_slots, iterator_end, m_control_bytes); }

    private:
        void erase_meta(const Iterator& it);

        detail::FindResult find_or_prepare_insert(const K& key);
        detail::FindInfo find_first_non_full(u64 hash);

        u64 prepare_insert(u64 hash);

        detail::ProbeSequence probe(u64 hash);
        void rehash_and_grow_if_necessary();

        void drop_deletes_whitout_resize();
        u64 calculate_size(u64 new_capacity);

        void initialize_slots();

        void resize(u64 new_capacity);

        void set_ctrl(u64 i, i8 h);
        void reset_ctrl();
        void reset_growth_left();

        i8* m_control_bytes;
        KeyValue* m_slots;

        u64 m_size;
        u64 m_capacity;
        u64 m_growth_left;

        Allocator* m_allocator;
        bool m_own_allocator;
    };

    // Public Map
    template <typename K, typename V>
    Map<K, V>::Map(Map&& other) noexcept {
        m_control_bytes = other.m_control_bytes;
        m_slots = other.m_slots;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        m_growth_left = other.m_growth_left;
        m_allocator = other.m_allocator;
        m_own_allocator = other.m_own_allocator;

        other.m_control_bytes = nullptr;
        other.m_slots = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
        other.m_growth_left = 0;
        other.m_allocator = nullptr;
        other.m_own_allocator = false;
    }

    template <typename K, typename V>
    Map<K, V>& Map<K, V>::operator=(Map&& right) noexcept {
        m_control_bytes = right.m_control_bytes;
        m_slots = right.m_slots;
        m_size = right.m_size;
        m_capacity = right.m_capacity;
        m_growth_left = right.m_growth_left;
        m_allocator = right.m_allocator;
        m_own_allocator = right.m_own_allocator;

        right.m_control_bytes = nullptr;
        right.m_slots = nullptr;
        right.m_size = 0;
        right.m_capacity = 0;
        right.m_growth_left = 0;
        right.m_allocator = nullptr;
        right.m_own_allocator = false;

        return *this;
    }

    template <typename K, typename V>
    Map<K, V>::Iterator Map<K, V>::find(const K& key) {
        const u64 hash = detail::hash_calculate(key);
        detail::ProbeSequence sequence = probe(hash);

        while (true) {
            const detail::GroupSse2Impl group{m_control_bytes + sequence.get_offset()};
            const i8 hash2 = detail::hash_2(hash);
            for (i32 i : group.match(hash2)) {
                const KeyValue& key_value = *(m_slots + sequence.get_offset(i));
                if (key_value.key == key) {
                    return {m_slots, sequence.get_offset(i), m_control_bytes};
                }
            }

            if (group.match_empty())
                break;

            sequence.next();
        }

        return {m_slots, iterator_end, m_control_bytes};
    }

    template <typename K, typename V>
    void Map<K, V>::insert(const K& key, const V& value) {
        const detail::FindResult find_result = find_or_prepare_insert(key);
        if (find_result.free_index) {
            // Emplace
            m_slots[find_result.index].key = key;
            m_slots[find_result.index].value = value;
        } else {
            // Substitute value index
            m_slots[find_result.index].value = value;
        }
    }

    template <typename K, typename V>
    V& Map<K, V>::insert_use(const K& key) {
        const detail::FindResult find_result = find_or_prepare_insert(key);
        if (find_result.free_index) {
            m_slots[find_result.index].key = key;
        }
        return m_slots[find_result.index].value;
    }

    template <typename K, typename V>
    void Map<K, V>::insert_move(const K& key, V& value) {
        const detail::FindResult find_result = find_or_prepare_insert(key);
        if (find_result.free_index) {
            // Emplace
            m_slots[find_result.index].key = key;
            m_slots[find_result.index].value = std::move(value);
        } else {
            // Substitute value index
            m_slots[find_result.index].value = std::move(value);
        }
    }

    template <typename K, typename V>
    bool Map<K, V>::remove(const K& key) {
        auto it = find(key);
        if (it == end())
            return false;

        erase_meta(it);
        return true;
    }

    template <typename K, typename V>
    bool Map<K, V>::remove(const Iterator& it) {
        if (it == end())
            return false;

        erase_meta(it);
        return true;
    }

    template <typename K, typename V>
    Map<K, V>::OptRefValue Map<K, V>::get(const K& key) {
        auto it = find(key);
        if (it != end())
            return std::ref((*it).value);

        return std::nullopt;
    }

    template <typename K, typename V>
    Map<K, V>::OptRefValue Map<K, V>::get(const Iterator& it) {
        if (it != end())
            return std::ref((*it).value);

        return std::nullopt;
    }

    template <typename K, typename V>
    Map<K, V>::OptRefValue Map<K, V>::operator[](const K& key) {
        return get(key);
    }

    template <typename K, typename V>
    Map<K, V>::OptRefKeyValue Map<K, V>::get_key_value(const K& key) {
        auto it = find(key);
        if (it != end())
            return std::ref(*it);

        return std::nullopt;
    }

    template <typename K, typename V>
    Map<K, V>::OptRefKeyValue Map<K, V>::get_key_value(const Iterator& it) {
        if (it != end())
            return std::ref(*it);

        return std::nullopt;
    }

    template <typename K, typename V>
    void Map<K, V>::clear() {
        m_size = 0;
        reset_ctrl();
        reset_growth_left();
    }

    template <typename K, typename V>
    void Map<K, V>::free() {
        if constexpr(std::is_class_v<V>) {
            for (auto it = begin(); it != end(); it++) {
                it->value.~V();
            }
        }
        m_allocator->free_raw(m_control_bytes, calculate_size(m_capacity));
    }

    template <typename K, typename V>
    void Map<K, V>::reserve(u64 new_size) {
        if (new_size <= m_size + m_growth_left)
            return;
        size_t m = detail::capacity_growth_to_lower_bound(new_size);
        size_t normalized = detail::capacity_normalize(m);
        resize(normalized);
    }

    // Private Map
    template <typename K, typename V>
    void Map<K, V>::erase_meta(const Iterator& it) {
        m_size--;

        const u64 index = it.index();
        const u64 index_before = (index - detail::GroupSse2Impl::width) & m_capacity;
        const auto empty_after = detail::GroupSse2Impl{m_control_bytes + index}.match_empty();
        const auto empty_before = detail::GroupSse2Impl{m_control_bytes + index_before}.match_empty();

        const u64 trailing_zeros = empty_after.trailing_zeros();
        const u64 leading_zeros = empty_before.leading_zeros();
        const u64 zeros = trailing_zeros + leading_zeros;
        bool was_never_full = empty_before && empty_after && (zeros < detail::GroupSse2Impl::width);

        set_ctrl(index, was_never_full ? detail::control_bitmask_empty : detail::control_bitmask_deleted);
        m_growth_left += was_never_full;
    }

    template <typename K, typename V>
    detail::FindResult Map<K, V>::find_or_prepare_insert(const K& key) {
        u64 hash = detail::hash_calculate(key);
        detail::ProbeSequence sequence = probe(hash);

        while (true) {
            const detail::GroupSse2Impl group{m_control_bytes + sequence.get_offset()};
            for (int i : group.match(detail::hash_2(hash))) {
                const KeyValue& key_value = *(m_slots + sequence.get_offset(i));
                if (key_value.key == key)
                    return {sequence.get_offset(i), false};
            }

            if (group.match_empty())
                break;

            sequence.next();
        }
        return {prepare_insert(hash), true};
    }

    template <typename K, typename V>
    detail::FindInfo Map<K, V>::find_first_non_full(u64 hash) {
        detail::ProbeSequence sequence = probe(hash);

        while (true) {
            const detail::GroupSse2Impl group{m_control_bytes + sequence.get_offset()};
            auto mask = group.match_empty_or_deleted();

            if (mask)
                return {sequence.get_offset(mask.lowest_bit_set()), sequence.get_index()};

            sequence.next();
        }

        return detail::FindInfo{};
    }

    template <typename K, typename V>
    u64 Map<K, V>::prepare_insert(u64 hash) {
        detail::FindInfo find_info = find_first_non_full(hash);
        if (m_growth_left == 0 && !detail::control_is_deleted(m_control_bytes[find_info.offset])) {
            rehash_and_grow_if_necessary();
            find_info = find_first_non_full(hash);
        }
        m_size++;

        m_growth_left -= detail::control_is_empty(m_control_bytes[find_info.offset]) ? 1 : 0;
        set_ctrl(find_info.offset, detail::hash_2(hash));
        return find_info.offset;
    }

    template <typename K, typename V>
    detail::ProbeSequence Map<K, V>::probe(u64 hash) {
        return detail::ProbeSequence{detail::hash_1(hash, m_control_bytes), m_capacity};
    }

    template <typename K, typename V>
    void Map<K, V>::rehash_and_grow_if_necessary() {
        if (m_capacity == 0) {
            resize(1);
        } else if (m_size <= detail::capacity_to_growth(m_capacity) / 2) {
            drop_deletes_whitout_resize();
        } else {
            resize(m_capacity * 2 + 1);
        }
    }

    template <typename K, typename V>
    void Map<K, V>::drop_deletes_whitout_resize() {
        alignas(KeyValue) unsigned char raw[sizeof(KeyValue)];
        size_t total_probe_length = 0;
        KeyValue* slot = reinterpret_cast<KeyValue*>(&raw);
        for (size_t i = 0; i != m_capacity; i++) {
            if (!detail::control_is_deleted(m_control_bytes[i]))
                continue;

            const KeyValue* current_slot = m_slots + i;
            size_t hash = detail::hash_calculate(current_slot->key);
            auto target = find_first_non_full(hash);
            size_t new_i = target.offset;
            total_probe_length += target.probe_length;

            const auto probe_index = [&](size_t pos) {
                return ((pos - probe(hash).get_offset()) & m_capacity) / detail::GroupSse2Impl::width;
            };

            if ((probe_index(new_i) == probe_index(i))) {
                set_ctrl(i, detail::hash_2(hash));
                continue;
            }

            if (detail::control_is_empty(m_control_bytes[new_i])) {
                set_ctrl(new_i, detail::hash_2(hash));
                memcpy(m_slots + new_i, m_slots + i, sizeof(KeyValue));
                set_ctrl(i, detail::control_bitmask_empty);
                continue;
            }

            set_ctrl(new_i, detail::hash_2(hash));
            memcpy(slot, m_slots + i, sizeof(KeyValue));
            memcpy(m_slots + i, m_slots + new_i, sizeof(KeyValue));
            memcpy(m_slots + new_i, slot, sizeof(KeyValue));
            i--;
        }
    }

    template <typename K, typename V>
    u64 Map<K, V>::calculate_size(u64 new_capacity) {
        return (new_capacity + detail::GroupSse2Impl::width + new_capacity * (sizeof(KeyValue)));
    }

    template <typename K, typename V>
    void Map<K, V>::initialize_slots() {
        char* new_memory = m_allocator->allocate_raw_t(char, calculate_size(m_capacity));

        m_control_bytes = reinterpret_cast<i8*>(new_memory);
        m_slots = reinterpret_cast<KeyValue*>(new_memory + m_capacity + detail::GroupSse2Impl::width);

        reset_ctrl();
        reset_growth_left();
    }

    template <typename K, typename V>
    void Map<K, V>::resize(u64 new_capacity) {
        i8* old_control_bytes = m_control_bytes;
        KeyValue* old_slots = m_slots;
        const u64 old_capacity = m_capacity;

        m_capacity = new_capacity;

        initialize_slots();

        size_t total_probe_length = 0;
        for (size_t i = 0; i != old_capacity; i++) {
            if (!detail::control_is_full(old_control_bytes[i]))
                continue;

            const KeyValue* old_value = old_slots + i;
            u64 hash = detail::hash_calculate(old_value->key);

            detail::FindInfo find_info = find_first_non_full(hash);

            u64 new_i = find_info.offset;
            total_probe_length += find_info.probe_length;

            set_ctrl(new_i, detail::hash_2(hash));
            memcpy(m_slots + new_i, old_slots + i, sizeof(KeyValue));
        }

        if (old_capacity != 0)
            m_allocator->free_raw(old_control_bytes, calculate_size(old_capacity));
    }

    template <typename K, typename V>
    void Map<K, V>::set_ctrl(u64 i, i8 h) {
        m_control_bytes[i] = h;
        constexpr size_t cloned_bytes = detail::GroupSse2Impl::width - 1;
        m_control_bytes[((i - cloned_bytes) & m_capacity) + (cloned_bytes & m_capacity)] = h;
    }

    template <typename K, typename V>
    void Map<K, V>::reset_ctrl() {
        memset(m_control_bytes, detail::control_bitmask_empty, m_capacity + detail::GroupSse2Impl::width);
        m_control_bytes[m_capacity] = detail::control_bitmask_sentinel;
    }

    template <typename K, typename V>
    void Map<K, V>::reset_growth_left() {
        m_growth_left = detail::capacity_to_growth(m_capacity) - m_size;
    }
}
