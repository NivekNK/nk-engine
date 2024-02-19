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

    using str = std::string;
    using cstr = const char*;
    using vstr = std::string_view;

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

#define TRUE 1
#define FALSE 0
