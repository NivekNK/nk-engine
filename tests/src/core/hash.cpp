#include <gtest/gtest.h>

#include "core/hash.h"
#include "core/str.h"
#include "memory/malloc_allocator.h"

TEST(Hash64, PreservesRapidhashV3Vectors) {
    EXPECT_EQ(nk::hash64("", nk::hash_seed::deterministic), 0x4c24cbaf06a9cb33ull);
    EXPECT_EQ(
        nk::hash64("nk-engine", nk::hash_seed::deterministic),
        0x73127fdbe7851652ull);
    EXPECT_EQ(
        nk::hash64("Images and Textures", nk::hash_seed::deterministic),
        0xb89843052581e81bull);
}

TEST(Hash64, AdaptsOwnedTextNumbersAndPointers) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::str text{allocator, "nk-engine"};

    EXPECT_EQ(nk::hash64(text), nk::hash64(text.view()));
    EXPECT_NE(nk::hash64(text, 1), nk::hash64(text, 2));

    const nk::u64 number = 42;
    EXPECT_EQ(nk::hash64(number), nk::hash64_bytes(&number, sizeof(number)));

    const nk::u64* pointer = &number;
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
    EXPECT_EQ(nk::hash64(pointer), nk::hash64_bytes(&address, sizeof(address)));
}
