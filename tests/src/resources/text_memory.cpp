#include <gtest/gtest.h>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include "resources/text_memory.h"

extern "C" void* hb_malloc_impl(std::size_t);
extern "C" void* hb_calloc_impl(std::size_t, std::size_t);
extern "C" void* hb_realloc_impl(void*, std::size_t);
extern "C" void hb_free_impl(void*);

TEST(TextMemory, TracksAlignedZeroedStorageAndPreservesFailedReallocation) {
    const auto baseline = nk::harfbuzz_memory_statistics();
    auto* block = static_cast<unsigned char*>(hb_calloc_impl(17, 3));
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(block) % alignof(std::max_align_t), 0u);
    for (int i = 0; i < 51; ++i)
        EXPECT_EQ(block[i], 0);
    std::memset(block, 0x5a, 51);
    EXPECT_EQ(hb_realloc_impl(block, SIZE_MAX), nullptr);
    auto* grown = static_cast<unsigned char*>(hb_realloc_impl(block, 99));
    ASSERT_NE(grown, nullptr);
    for (int i = 0; i < 51; ++i)
        EXPECT_EQ(grown[i], 0x5a);
    EXPECT_EQ(nk::harfbuzz_memory_statistics().live_bytes, baseline.live_bytes + 99);
    EXPECT_EQ(hb_realloc_impl(grown, 0), nullptr);
    EXPECT_EQ(hb_calloc_impl(SIZE_MAX, 2), nullptr);
    EXPECT_EQ(nk::harfbuzz_memory_statistics().live_bytes, baseline.live_bytes);
    EXPECT_EQ(nk::harfbuzz_memory_statistics().live_blocks, baseline.live_blocks);
}
