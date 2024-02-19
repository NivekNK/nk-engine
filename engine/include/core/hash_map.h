#pragma once

#include "memory/allocator.h"
#include "wy/wyhash.h"

namespace nk {
    namespace detail {
        struct FindResult;
        struct FindInfo;
        class ProbeSequence;
        static i8* group_init_empty();
    }

    template <typename K, typename V>
    class HashMap {
    public:
        static constexpr const u64 iterator_end = u64_max;

        struct KeyValue {
            K key;
            V value;
        };

        struct Iterator {
            u64 index;
            inline bool is_valid() const { return index != iterator_end; }
            inline bool is_invalid() const { return index == iterator_end; }
        };

        HashMap(Allocator* allocator, const u64 initial_capacity);
        ~HashMap();

        Iterator find(const K& key);
        void insert(const K& key, const V& value);
        bool remove(const K& key);
        bool remove(const Iterator& it);

        using OptRefValue = std::optional<std::reference_wrapper<V>>;
        OptRefValue get(const K& key);
        OptRefValue get(const Iterator& it);

        using OptRefKeyValue = std::optional<std::reference_wrapper<KeyValue>>;
        OptRefKeyValue get_key_value(const K& key);
        OptRefKeyValue get_key_value(const Iterator& it);

        Iterator it_begin();
        void it_advance(Iterator& it);

        void clear();
        void free();

        void reserve(u64 new_size);
        u64 size() const { return m_size; }

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

        void iterator_skip_empty_or_deleted(Iterator& it);

        void set_ctrl(u64 i, i8 h);
        void reset_ctrl();
        void reset_growth_left();

        i8* m_control_bytes = detail::group_init_empty();
        KeyValue* m_slots = nullptr;

        u64 m_size = 0;
        u64 m_capacity = 0;
        u64 m_growth_left = 0;

        Allocator* m_allocator = nullptr;
    };

    namespace detail {
        // Hash Functions
        struct FindResult {
            u64 index;
            bool free_index;
        };

        struct FindInfo {
            u64 offset;
            u64 probe_length;
        };

        class ProbeSequence {
        public:
            static constexpr const u64 width = 16;
            static constexpr const size_t engine_hash = 0x31d3a36013e;

            inline ProbeSequence(u64 hash, u64 mask)
                : m_mask{mask}, m_offset{hash & mask} {
            }

            inline u64 get_offset() const { return m_offset; }

            inline u64 get_offset(u64 i) const { return (m_offset + i) & m_mask; }

            inline u64 get_index() const { return m_index; }

            inline void next() {
                m_index = width;
                m_offset += m_index;
                m_offset &= m_mask;
            }

        private:
            u64 m_mask;
            u64 m_offset;
            u64 m_index = 0;
        };

        template <typename T>
        inline u64 hash_calculate(const T& value, size_t seed = 0) {
            return wyhash(&value, sizeof(T), seed, _wyp);
        }

        template <size_t N>
        inline u64 hash_calculate(const char (&value)[N], size_t seed = 0) {
            return wyhash(value, strlen(value), seed, _wyp);
        }

        template <>
        inline u64 hash_calculate(const cstr& value, size_t seed) {
            return wyhash(value, strlen(value), seed, _wyp);
        }

        inline u64 hash_bytes(void* data, size_t length, size_t seed = 0) {
            return wyhash(data, length, seed, _wyp);
        }

        // Control byte
        static constexpr const i8 control_bitmask_empty = -128;
        static constexpr const i8 control_bitmask_deleted = -2;
        static constexpr const i8 control_bitmask_sentinel = -1;

        static bool control_is_empty(const i8 control) { return control == control_bitmask_empty; }
        static bool control_is_full(const i8 control) { return control >= 0; }
        static bool control_is_deleted(const i8 control) { return control == control_bitmask_deleted; }
        static bool control_is_empty_or_deleted(const i8 control) { return control < control_bitmask_sentinel; }

        i8* group_init_empty() {
            alignas(16) static constexpr i8 empty_group[] = {
                control_bitmask_sentinel,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
                control_bitmask_empty,
            };
            return const_cast<i8*>(empty_group);
        }

        // Hashing
        static u64 hash_seed(const i8* control) { return reinterpret_cast<uintptr_t>(control) >> 12; }
        static u64 hash_1(u64 hash, const i8* ctrl) { return (hash >> 7) ^ hash_seed(ctrl); }
        static i8 hash_2(u64 hash) { return hash & 0x7F; }

        static u32 leading_zeros_u32(u32 x) { return __builtin_clz(x); }
        static u32 trailing_zeros_u32(u32 x) { return __builtin_ctz(x); }

        template <class T, int SignificantBits, int Shift = 0>
        class BitMask {
        public:
            using value_type = i32;
            using iterator = BitMask;
            using const_iterator = BitMask;

            explicit BitMask(T mask) : m_mask(mask) {}

            BitMask& operator++() {
                m_mask &= (m_mask - 1);
                return *this;
            }

            explicit operator bool() const { return m_mask != 0; }
            int operator*() const { return lowest_bit_set(); }

            u32 lowest_bit_set() const {
                return trailing_zeros_u32(m_mask) >> Shift;
            }
            u32 highest_bit_set() const {
                return static_cast<u32>((bit_width(m_mask) - 1) >> Shift);
            }

            BitMask begin() const { return *this; }
            BitMask end() const { return BitMask(0); }

            u32 trailing_zeros() const { return trailing_zeros_u32(m_mask); }
            u32 leading_zeros() const { return leading_zeros_u32(m_mask); }

        private:
            friend bool operator==(const BitMask& a, const BitMask& b) {
                return a.m_mask == b.m_mask;
            }

            friend bool operator!=(const BitMask& a, const BitMask& b) {
                return a.m_mask != b.m_mask;
            }

            T m_mask;
        };

        class GroupSse2Impl {
        public:
            static constexpr size_t width = 16; // the number of slots per group

            explicit GroupSse2Impl(const i8* pos) {
                ctrl = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pos));
            }

            BitMask<u32, width> match(i8 hash) const {
                auto match = _mm_set1_epi8(hash);
                return BitMask<u32, width>(
                    _mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));
            }

            BitMask<u32, width> match_empty() const {
                return match(static_cast<i8>(control_bitmask_empty));
            }

            BitMask<u32, width> match_empty_or_deleted() const {
                auto special = _mm_set1_epi8(control_bitmask_sentinel);
                return BitMask<u32, width>(
                    _mm_movemask_epi8(_mm_cmpgt_epi8(special, ctrl)));
            }

            u32 count_leading_empty_or_deleted() const {
                auto special = _mm_set1_epi8(control_bitmask_sentinel);
                return trailing_zeros_u32(static_cast<u32>(
                    _mm_movemask_epi8(_mm_cmpgt_epi8(special, ctrl)) + 1));
            }

            void convert_special_to_empty_and_full_to_deleted(i8* dst) const {
                auto msbs = _mm_set1_epi8(static_cast<char>(-128));
                auto x126 = _mm_set1_epi8(126);
                auto zero = _mm_setzero_si128();
                auto special_mask = _mm_cmpgt_epi8(zero, ctrl);
                auto res = _mm_or_si128(msbs, _mm_andnot_si128(special_mask, x126));
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), res);
            }

        private:
            __m128i ctrl;
        };

        static bool capacity_is_valid(size_t n) { return ((n + 1) & n) == 0 && n > 0; }
        static u64 lzcnt_soft(u64 n) {
#if defined(NK_PLATFORM_WINDOWS)
            unsigned long index = 0;
            _BitScanReverse64(&index, n);
            u64 cnt = index ^ 63;
#else
            u64 cnt = __builtin_clzl(n);
#endif
            return cnt;
        }
        static u64 capacity_normalize(u64 n) { return n ? ~u64{} >> lzcnt_soft(n) : 1; }
        static u64 capacity_to_growth(u64 capacity) { return capacity - capacity / 8; }
        static u64 capacity_growth_to_lower_bound(u64 growth) { return growth + static_cast<u64>((static_cast<i64>(growth) - 1) / 7); }

        static void convert_deleted_to_empty_and_full_to_deleted(i8* ctrl, size_t capacity) {
            for (i8* pos = ctrl; pos != ctrl + capacity + 1; pos += GroupSse2Impl::width) {
                GroupSse2Impl{pos}.convert_special_to_empty_and_full_to_deleted(pos);
            }
            memcpy(ctrl + capacity + 1, ctrl, GroupSse2Impl::width);
            ctrl[capacity] = control_bitmask_sentinel;
        }
    }

    // Public HashMap
    template <typename K, typename V>
    HashMap<K, V>::HashMap(Allocator* allocator, const u64 initial_capacity)
        : m_allocator{allocator},
          m_size{0},
          m_capacity{0},
          m_growth_left{0},
          m_control_bytes{detail::group_init_empty()},
          m_slots{nullptr} {
        reserve(initial_capacity < 4 ? 4 : initial_capacity);
    }

    template <typename K, typename V>
    HashMap<K, V>::~HashMap() {
        m_allocator->free_raw(m_control_bytes, calculate_size(m_capacity));
    }

    template <typename K, typename V>
    HashMap<K, V>::Iterator HashMap<K, V>::find(const K& key) {
        const u64 hash = detail::hash_calculate(key);
        detail::ProbeSequence sequence = probe(hash);

        while (true) {
            const detail::GroupSse2Impl group{m_control_bytes + sequence.get_offset()};
            const i8 hash2 = detail::hash_2(hash);
            for (i32 i : group.match(hash2)) {
                const KeyValue& key_value = *(m_slots + sequence.get_offset(i));
                if (key_value.key == key)
                    return {sequence.get_offset(i)};
            }

            if (group.match_empty())
                break;

            sequence.next();
        }

        return {iterator_end};
    }

    template <typename K, typename V>
    void HashMap<K, V>::insert(const K& key, const V& value) {
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
    bool HashMap<K, V>::remove(const K& key) {
        auto it = find(key);
        if (it.index == iterator_end)
            return false;

        erase_meta(it);
        return true;
    }

    template <typename K, typename V>
    inline bool HashMap<K, V>::remove(const Iterator& it) {
        if (it.index == iterator_end)
            return false;

        erase_meta(it);
        return true;
    }

    template <typename K, typename V>
    HashMap<K, V>::OptRefValue HashMap<K, V>::get(const K& key) {
        auto it = find(key);
        if (it.index != iterator_end)
            return std::ref(m_slots[it.index].value);

        return std::nullopt;
    }

    template <typename K, typename V>
    HashMap<K, V>::OptRefValue HashMap<K, V>::get(const Iterator& it) {
        if (it.index != iterator_end)
            return std::ref(m_slots[it.index].value);

        return std::nullopt;
    }

    template <typename K, typename V>
    HashMap<K, V>::OptRefKeyValue HashMap<K, V>::get_key_value(const K& key) {
        auto it = find(key);
        if (it.index != iterator_end)
            return std::ref(m_slots[it.index]);

        return std::nullopt;
    }

    template <typename K, typename V>
    HashMap<K, V>::OptRefKeyValue HashMap<K, V>::get_key_value(const Iterator& it) {
        if (it.index != iterator_end)
            return std::ref(m_slots[it.index]);

        return std::nullopt;
    }

    template <typename K, typename V>
    HashMap<K, V>::Iterator HashMap<K, V>::it_begin() {
        HashMap<K, V>::Iterator it{0};
        iterator_skip_empty_or_deleted(it);
        return it;
    }

    template <typename K, typename V>
    void HashMap<K, V>::it_advance(Iterator& it) {
        it.index++;
        iterator_skip_empty_or_deleted(it);
    }

    template <typename K, typename V>
    void HashMap<K, V>::clear() {
        m_size = 0;
        reset_ctrl();
        reset_growth_left();
    }

    template <typename K, typename V>
    void HashMap<K, V>::free() {
        m_allocator->free_raw(m_control_bytes, calculate_size(m_capacity));
    }

    template <typename K, typename V>
    void HashMap<K, V>::reserve(u64 new_size) {
        if (new_size <= m_size + m_growth_left)
            return;
        size_t m = detail::capacity_growth_to_lower_bound(new_size);
        size_t normalized = detail::capacity_normalize(m);
        resize(normalized);
    }

    // Private HashMap
    template <typename K, typename V>
    void HashMap<K, V>::erase_meta(const HashMap<K, V>::Iterator& it) {
        m_size--;

        const u64 index = iterator_end;
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
    detail::FindResult HashMap<K, V>::find_or_prepare_insert(const K& key) {
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
    detail::FindInfo HashMap<K, V>::find_first_non_full(u64 hash) {
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
    u64 HashMap<K, V>::prepare_insert(u64 hash) {
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
    detail::ProbeSequence HashMap<K, V>::probe(u64 hash) {
        return detail::ProbeSequence{detail::hash_1(hash, m_control_bytes), m_capacity};
    }

    template <typename K, typename V>
    void HashMap<K, V>::rehash_and_grow_if_necessary() {
        if (m_capacity == 0) {
            resize(1);
        } else if (m_size <= detail::capacity_to_growth(m_capacity) / 2) {
            drop_deletes_whitout_resize();
        } else {
            resize(m_capacity * 2 + 1);
        }
    }

    template <typename K, typename V>
    void HashMap<K, V>::drop_deletes_whitout_resize() {
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
    u64 HashMap<K, V>::calculate_size(u64 new_capacity) {
        return (new_capacity + detail::GroupSse2Impl::width + new_capacity * (sizeof(KeyValue)));
    }

    template <typename K, typename V>
    void HashMap<K, V>::initialize_slots() {
        char* new_memory = m_allocator->allocate_raw_t(char, calculate_size(m_capacity));

        m_control_bytes = reinterpret_cast<i8*>(new_memory);
        m_slots = reinterpret_cast<KeyValue*>(new_memory + m_capacity + detail::GroupSse2Impl::width);

        reset_ctrl();
        reset_growth_left();
    }

    template <typename K, typename V>
    void HashMap<K, V>::resize(u64 new_capacity) {
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
    void HashMap<K, V>::iterator_skip_empty_or_deleted(HashMap<K, V>::Iterator& it) {
        i8* ctrl = m_control_bytes + it.index;

        while (detail::control_is_empty_or_deleted(*ctrl)) {
            u32 shift = detail::GroupSse2Impl{ctrl}.count_leading_empty_or_deleted();
            ctrl += shift;
            it.index += shift;
        }

        if (*ctrl == detail::control_bitmask_sentinel)
            it.index = iterator_end;
    }

    template <typename K, typename V>
    void HashMap<K, V>::set_ctrl(u64 i, i8 h) {
        m_control_bytes[i] = h;
        constexpr size_t cloned_bytes = detail::GroupSse2Impl::width - 1;
        m_control_bytes[((i - cloned_bytes) & m_capacity) + (cloned_bytes & m_capacity)] = h;
    }

    template <typename K, typename V>
    void HashMap<K, V>::reset_ctrl() {
        memset(m_control_bytes, detail::control_bitmask_empty, m_capacity + detail::GroupSse2Impl::width);
        m_control_bytes[m_capacity] = detail::control_bitmask_sentinel;
    }

    template <typename K, typename V>
    void HashMap<K, V>::reset_growth_left() {
        m_growth_left = detail::capacity_to_growth(m_capacity) - m_size;
    }
}

