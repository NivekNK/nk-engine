#include <type_traits>

#include <gtest/gtest.h>

#include "collections/arr.h"
#include "collections/dyarr.h"
#include "collections/slice.h"
#include "memory/malloc_allocator.h"

namespace {
    static_assert(sizeof(nk::cl::slice<nk::u32>) == sizeof(void*) + sizeof(nk::u64));
    static_assert(std::is_constructible_v<
                  nk::cl::slice<const nk::u32>,
                  nk::cl::slice<nk::u32>>);
    static_assert(!std::is_constructible_v<
                  nk::cl::slice<nk::u32>,
                  nk::cl::slice<const nk::u32>>);
}

TEST(Slice, ViewsPointersAndCArrayStorage) {
    nk::u32 values[] = {2, 4, 6, 8, 10};
    nk::cl::slice<nk::u32> view{values};

    ASSERT_EQ(view.length(), 5);
    EXPECT_EQ(view.first(), 2u);
    EXPECT_EQ(view.last(), 10u);
    view[2] = 7;
    EXPECT_EQ(values[2], 7u);

    nk::cl::slice<const nk::u32> readonly{view};
    EXPECT_EQ(readonly.subslice(1, 3).length(), 3);
    EXPECT_EQ(readonly.subslice(1, 3)[0], 4u);
    EXPECT_EQ(readonly.subslice(1, 3)[2], 8u);
    EXPECT_TRUE(readonly.subslice(20).empty());
}

TEST(Slice, ViewsFixedAndDynamicArraysWithoutOwnership) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::arr<nk::u32> fixed;
    nk::cl::dyarr<nk::u32> dynamic;
    ASSERT_TRUE(fixed.arr_init_list(&allocator, {1u, 2u, 3u}));
    ASSERT_TRUE(dynamic.dyarr_init_list(&allocator, {5u, 6u, 7u, 8u}));

    {
        nk::cl::slice<nk::u32> fixed_view{fixed};
        nk::cl::slice<nk::u32> dynamic_view{dynamic};
        fixed_view.last() = 4;
        dynamic_view.first() = 9;
        EXPECT_EQ(fixed[2], 4u);
        EXPECT_EQ(dynamic[0], 9u);

        nk::u32 sum = 0;
        for (const nk::u32 value : dynamic_view)
            sum += value;
        EXPECT_EQ(sum, 30u);
    }

    EXPECT_EQ(allocator.get_active_allocation_count(), 2);
    EXPECT_TRUE(dynamic.dyarr_shutdown());
    EXPECT_TRUE(fixed.arr_shutdown());
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}
