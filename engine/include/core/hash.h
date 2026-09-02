#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/defines.h"
#include "core/strview.h"

namespace nk {
    class str;

    namespace hash_seed {
        inline constexpr u64 deterministic = 0x6a09e667f3bcc909ull;
        inline constexpr u64 memory_allocations = 0xbb67ae8584caa73bull;
        inline constexpr u64 memory_allocators = 0x3c6ef372fe94f82bull;
    }

    u64 hash64_bytes(
        const void* data,
        u64 size_bytes,
        u64 seed = hash_seed::deterministic) noexcept;

    inline u64 hash64(
        const strview value,
        const u64 seed = hash_seed::deterministic) noexcept {
        return hash64_bytes(value.data(), value.length(), seed);
    }

    inline u64 hash64(
        const cstr value,
        const u64 seed = hash_seed::deterministic) noexcept {
        return hash64(strview{value}, seed);
    }

    u64 hash64(
        const str& value,
        u64 seed = hash_seed::deterministic) noexcept;

    template <typename T>
        requires (std::integral<T> || std::is_enum_v<T>)
    inline u64 hash64(
        const T value,
        const u64 seed = hash_seed::deterministic) noexcept {
        return hash64_bytes(&value, sizeof(value), seed);
    }

    template <typename T>
        requires (!std::same_as<std::remove_cv_t<T>, char>)
    inline u64 hash64(
        T* const value,
        const u64 seed = hash_seed::deterministic) noexcept {
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(value);
        return hash64_bytes(&address, sizeof(address), seed);
    }
}
