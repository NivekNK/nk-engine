#pragma once

namespace nk {
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using f32 = float;
    using f64 = double;

    using cstr = const char*;

    namespace numeric {
        static constexpr const u8 u8_max = UINT8_MAX;
        static constexpr const u16 u16_max = UINT16_MAX;
        static constexpr const u32 u32_max = UINT32_MAX;
        static constexpr const u64 u64_max = UINT64_MAX;

        static constexpr const i8 i8_max = INT8_MAX;
        static constexpr const i16 i16_max = INT16_MAX;
        static constexpr const i32 i32_max = INT32_MAX;
        static constexpr const i64 i64_max = INT64_MAX;

        static constexpr const f32 f32_max = std::numeric_limits<float>::max();
        static constexpr const f64 f64_max = std::numeric_limits<double>::max();
    }

    namespace mem {
        template <typename T>
        static void realocate_n(T* src, T* dst, size_t n) {
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memmove(dst, src, n * sizeof(T));
            } else {
                for (size_t i = 0; i < n; i++) {
                    std::construct_at(dst + i, std::move(src[i]));
                    std::destroy_at(src + i);
                }
            }
        }
    }
}

#define TRUE                   1
#define FALSE                  0

#define GiB(...)               1073741824 __VA_OPT__(*) NK_1ST_ARGUMENT(__VA_ARGS__)
#define MiB(...)               1048576 __VA_OPT__(*) NK_1ST_ARGUMENT(__VA_ARGS__)
#define KiB(...)               1024 __VA_OPT__(*) NK_1ST_ARGUMENT(__VA_ARGS__)

#define MinValue(a, b)         (((a) < (b)) ? (a) : (b))
#define MaxValue(a, b)         (((a) > (b)) ? (a) : (b))

#define Clamp(value, min, max) (value <= min) ? min : (value >= max) ? max \
                                                                     : value;
