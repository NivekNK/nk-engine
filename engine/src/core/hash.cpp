#include "nkpch.h"

#include "core/hash.h"
#include "core/str.h"

// Keep the third-party macros and symbols out of the engine's public headers.
#include "rapidhash/rapidhash.h"

namespace nk {
    u64 hash64_bytes(
        const void* data,
        const u64 size_bytes,
        const u64 seed) noexcept {
        static constexpr u8 empty = 0;
        const void* bytes = data == nullptr ? &empty : data;
        return rapidhash_withSeed(
            bytes,
            static_cast<std::size_t>(size_bytes),
            seed);
    }

    u64 hash64(const str& value, const u64 seed) noexcept {
        return hash64(value.view(), seed);
    }
}
