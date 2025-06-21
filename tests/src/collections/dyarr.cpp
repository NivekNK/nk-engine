#include <gtest/gtest.h>

#undef NK_ACTIVE_MEMORY_SYSTEM
#define NK_ACTIVE_MEMORY_SYSTEM FALSE

#include "collections/dyarr.h"
#include "memory/malloc_allocator.h"

struct DyarrTest {
    nk::u32 value;
};

TEST(Arr, DyarrInit) {
    auto array = nk::cl::dyarr<DyarrTest>();

    nk::mem::MallocAllocator allocator;
    array.dyarr_init(&allocator, 4);

    array.dyarr_push_copy(DyarrTest {
        .value = 1234,
    });

    auto test = DyarrTest {
        .value = 1234567,
    };
    array.dyarr_push(test);

    DebugLog("Before Insert Capacity: {}", array.capacity());
    for (nk::u8 i = 0; i < array.length(); i++) {
        InfoLog("{}: {}", i, array[i].value);
    }

    array.dyarr_insert_copy(1, DyarrTest {
        .value = 21345,
    });

    array.dyarr_insert_copy(5, DyarrTest {
        .value = 12,
    });

    DebugLog("After Insert Capacity: {}", array.capacity());
    for (nk::u8 i = 0; i < array.length(); i++) {
        InfoLog("{}: {}", i, array[i].value);
    }

    array.dyarr_shutdown();
}
