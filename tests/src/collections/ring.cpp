#include <gtest/gtest.h>

#include "collections/ring.h"
#include "memory/malloc_allocator.h"

namespace {
    struct MoveOnly {
        static inline int live = 0;

        explicit MoveOnly(const int value) noexcept : value{value} {
            ++live;
        }
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
        MoveOnly(MoveOnly&& other) noexcept : value{other.value} {
            other.value = -1;
            ++live;
        }
        MoveOnly& operator=(MoveOnly&&) = delete;
        ~MoveOnly() noexcept { --live; }

        int value;
    };

    class RejectingAllocator final : public nk::mem::Allocator {
    public:
        RejectingAllocator() {
            (void)_allocator_init_untracked<RejectingAllocator>();
        }

        void init() {}
        nk::cstr to_cstr() const noexcept override {
            return "RejectingAllocator";
        }

    protected:
        void* _do_allocate(nk::u64, nk::u64) noexcept override {
            return nullptr;
        }
        bool _do_free(void*, nk::u64) noexcept override { return false; }
    };
}

TEST(Ring, PreservesFifoOrderAcrossWrapAround) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::ring<int> values;
    ASSERT_TRUE(values.ring_init(&allocator, 3));

    ASSERT_TRUE(values.emplace(1));
    ASSERT_TRUE(values.emplace(2));
    ASSERT_TRUE(values.emplace(3));
    EXPECT_TRUE(values.full());
    auto rejected = values.emplace(4);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error(), nk::cl::ring_error::full);
    EXPECT_EQ(values.length(), 3u);

    auto first = values.pop();
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, 1);
    ASSERT_TRUE(values.emplace(4));

    for (const int expected : {2, 3, 4}) {
        auto value = values.pop();
        ASSERT_TRUE(value);
        EXPECT_EQ(*value, expected);
    }
    EXPECT_TRUE(values.empty());
    auto empty = values.pop();
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error(), nk::cl::ring_error::empty);
    EXPECT_EQ(values.length(), 0u);
    EXPECT_TRUE(values.ring_shutdown());
}

TEST(Ring, OwnsMoveOnlyElementLifetimes) {
    MoveOnly::live = 0;
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    {
        nk::cl::ring<MoveOnly> values;
        ASSERT_TRUE(values.ring_init(&allocator, 2));
        ASSERT_TRUE(values.emplace(11));
        MoveOnly second{22};
        ASSERT_TRUE(values.push(std::move(second)));
        EXPECT_EQ(MoveOnly::live, 3);

        auto popped = values.pop();
        ASSERT_TRUE(popped);
        EXPECT_EQ(popped->value, 11);
        EXPECT_EQ(values.front()->value, 22);
        values.clear();
        EXPECT_TRUE(values.empty());
        EXPECT_EQ(MoveOnly::live, 2);
    }
    EXPECT_EQ(MoveOnly::live, 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(Ring, RejectsInvalidInitializationAndRollsBackOom) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::ring<int> values;
    EXPECT_FALSE(values.ring_init(nullptr, 4));
    EXPECT_FALSE(values.ring_init(&allocator, 0));

    RejectingAllocator rejecting;
    auto failed = values.ring_init(&rejecting, 4);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nk::cl::ring_error::out_of_memory);
    EXPECT_EQ(values.capacity(), 0u);
    EXPECT_EQ(values.allocator(), nullptr);

    ASSERT_TRUE(values.ring_init(&allocator, 4));
    EXPECT_FALSE(values.ring_init(&allocator, 4));
    EXPECT_TRUE(values.ring_shutdown());
    EXPECT_TRUE(values.ring_shutdown());
}

TEST(Ring, MatchesAReferenceModelForDeterministicRandomOperations) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::ring<nk::u32> values;
    ASSERT_TRUE(values.ring_init(&allocator, 17));

    nk::u32 model[17]{};
    nk::u32 model_head = 0;
    nk::u32 model_count = 0;
    nk::u32 random = 0x7182a3b4u;
    for (nk::u32 iteration = 0; iteration < 10000; ++iteration) {
        random = random * 1664525u + 1013904223u;
        const bool push = (random & 1u) != 0u;
        if (push && model_count < 17) {
            const nk::u32 value = random ^ iteration;
            ASSERT_TRUE(values.push_copy(value));
            model[(model_head + model_count) % 17] = value;
            ++model_count;
        } else if (!push && model_count > 0) {
            auto value = values.pop();
            ASSERT_TRUE(value);
            EXPECT_EQ(*value, model[model_head]);
            model_head = (model_head + 1) % 17;
            --model_count;
        }
        EXPECT_EQ(values.length(), model_count);
    }
}
