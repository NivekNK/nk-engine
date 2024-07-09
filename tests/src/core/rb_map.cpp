#include <gtest/gtest.h>
#include <benchmark/benchmark.h>

#include <core/rb_map.h>
#include <memory/malloc_allocator.h>

TEST(Map, InsertToMapNoGrow) {
    nk::MallocAllocator allocator;
    allocator.allocator_init("Test", nk::MemoryType::None);

    nk::Map<nk::str, nk::u32> map;
    map.init(&allocator);

    map.insert("Test_Key", 12);
    map.insert("Test_Key_1", 98);
    map.insert("Test_Key_2", 123);
    map.insert("Test_Key_3", 878);
    map.insert("AzulColor", 980);
    map.insert("Test_Key", 5);

    auto buckets = map.buckets();
    for (nk::u32 i = 0; i < map.capacity(); i++) {
        DebugLog("Used: {} | Key: {} | Value: {} | Distance: {}",
                 buckets[i].used, buckets[i].key, buckets[i].value, buckets[i].distance);
    }

    EXPECT_EQ(map.get("Test_Key").value_or_default(), 5);
    EXPECT_EQ(map.get("Test_Key_1").value_or_default(), 98);
    EXPECT_EQ(map.get("Test_Key_2").value_or_default(), 123);
    EXPECT_EQ(map.get("Test_Key_3").value_or_default(), 878);
    EXPECT_EQ(map.get("AzulColor").value_or_default(), 980);
}
