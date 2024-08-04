#include <gtest/gtest.h>
#include <benchmark/benchmark.h>

#include <core/dyarr.h>
#include <memory/malloc_allocator.h>

struct DyarrValue {
    nk::u32 value;
};

class DyarrTest : public testing::Test {
protected:
    DyarrTest() {
        if (!init) {
            allocator.allocator_init("Test", nk::MemoryType::None);
            init = true;
        }
    }

    static bool init;
    static nk::MallocAllocator allocator;
    static nk::Dyarr<DyarrValue> test1;
};

// Initialize the shared value
bool DyarrTest::init = false;
nk::MallocAllocator DyarrTest::allocator = {};
nk::Dyarr<DyarrValue> DyarrTest::test1 = {};

TEST_F(DyarrTest, Init) {
    test1.init(&allocator, 0);

    DebugLog("Capacity: {}", test1.capacity());

    EXPECT_EQ(test1.capacity(), 4);
}

TEST_F(DyarrTest, PushCopy) {
    test1.push_copy(DyarrValue {
        .value = 67
    });

    test1.push_copy(DyarrValue {
        .value = 32
    });

    test1.push_copy(DyarrValue {
        .value = 78
    });

    for (nk::u32 i = 0; i < test1.length(); i++) {
        DebugLog("[{}]: {}", i, test1[i].value);
    }

    EXPECT_EQ(test1[0].value, 67);
    EXPECT_EQ(test1[1].value, 32);
    EXPECT_EQ(test1[2].value, 78);
}

TEST_F(DyarrTest, Push) {
    auto dyarr_test = DyarrValue {
        .value = 100,
    };
    test1.push(dyarr_test);

    dyarr_test = DyarrValue {
        .value = 34,
    };
    test1.push(dyarr_test);

    for (nk::u32 i = 0; i < test1.length(); i++) {
        DebugLog("[{}]: {}", i, test1[i].value);
    }

    EXPECT_EQ(test1[3].value, 100);
    EXPECT_EQ(test1[4].value, 34);
}

TEST_F(DyarrTest, InsertCopy) {
    test1.insert_copy(7, DyarrValue {
        .value = 2312
    });

    for (nk::u32 i = 0; i < test1.length(); i++) {
        DebugLog("[{}]: {}", i, test1[i].value);
    }

    EXPECT_EQ(test1[7].value, 2312);
}

TEST_F(DyarrTest, Insert) {
    auto dyarr_test_2 = DyarrValue {
        .value = 23,
    };
    test1.insert(4, dyarr_test_2);

    for (nk::u32 i = 0; i < test1.length(); i++) {
        DebugLog("[{}]: {}", i, test1[i].value);
    }

    EXPECT_EQ(test1[4].value, 23);
}

TEST_F(DyarrTest, AccessOperator) {
    auto value = DyarrValue {
        .value = 777,
    };
    test1[7] = std::move(value);

    for (nk::u32 i = 0; i < test1.length(); i++) {
        DebugLog("[{}]: {}", i, test1[i].value);
    }

    EXPECT_EQ(test1[7].value, 777);
}

TEST_F(DyarrTest, AccessOperatorOver) {
    auto value = DyarrValue {
        .value = 788,
    };
    test1[11] = std::move(value);

    for (nk::u32 i = 0; i < test1.length(); i++) {
        DebugLog("[{}]: {}", i, test1[i].value);
    }

    EXPECT_EQ(test1[11].value, 788);
}

TEST_F(DyarrTest, AccessOperatorOverResize) {
    auto value = DyarrValue {
        .value = 999,
    };
    test1[15] = std::move(value);

    for (nk::u32 i = 0; i < test1.length(); i++) {
        DebugLog("[{}]: {}", i, test1[i].value);
    }

    EXPECT_EQ(test1[15].value, 999);
}

