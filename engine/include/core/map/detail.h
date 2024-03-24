#pragma once

#include <wy/wyhash.h>

namespace nk::detail {
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

    static i8* group_init_empty() {
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
