#include <gtest/gtest.h>

#include "collections/arr.h"
#include "memory/malloc_allocator.h"

TEST(Arr, ArrInit) {
    auto array = nk::cl::arr<nk::u32>();

    nk::mem::MallocAllocator allocator;
    array._arr_init(&allocator, 10);

    for (nk::u8 i = 0; i < 10; i++) {
        InfoLog("{}: {}", i, array[i]);
    }

    array._arr_clear();
}
