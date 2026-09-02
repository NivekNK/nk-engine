#include <cstdint>
#include <utility>

#include <gtest/gtest.h>

#include "collections/map.h"
#include "core/str.h"
#include "memory/malloc_allocator.h"

namespace map_test {
    struct CollisionKey {
        nk::u32 value;

        bool operator==(const CollisionKey&) const noexcept = default;
    };

    inline nk::u64 hash64(const CollisionKey&, const nk::u64 seed) noexcept {
        return seed ^ 3;
    }

    struct WrapKey {
        nk::u32 value;

        bool operator==(const WrapKey&) const noexcept = default;
    };

    inline nk::u64 hash64(const WrapKey&, const nk::u64) noexcept {
        return nk::numeric::u64_max;
    }

    struct alignas(64) OveralignedValue {
        explicit OveralignedValue(const nk::u64 initial_value) noexcept
            : value{initial_value} {}

        OveralignedValue(const OveralignedValue&) = delete;
        OveralignedValue& operator=(const OveralignedValue&) = delete;
        OveralignedValue(OveralignedValue&&) noexcept = default;
        OveralignedValue& operator=(OveralignedValue&&) = delete;

        nk::u64 value;
    };

    struct MoveOnly {
        static inline int live_count = 0;
        static inline int destruction_count = 0;

        explicit MoveOnly(const int initial_value) noexcept
            : value{initial_value} {
            ++live_count;
        }

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;

        MoveOnly(MoveOnly&& other) noexcept
            : value{other.value} {
            other.value = -1;
            ++live_count;
        }

        MoveOnly& operator=(MoveOnly&&) = delete;

        ~MoveOnly() noexcept {
            --live_count;
            ++destruction_count;
        }

        static void reset() noexcept {
            live_count = 0;
            destruction_count = 0;
        }

        int value;
    };

    struct ConstructionProbe {
        static inline int construction_count = 0;

        ConstructionProbe(const int left, const int right) noexcept
            : value{left + right} {
            ++construction_count;
        }

        ConstructionProbe(const ConstructionProbe&) = delete;
        ConstructionProbe& operator=(const ConstructionProbe&) = delete;

        ConstructionProbe(ConstructionProbe&& other) noexcept
            : value{other.value} {}

        ConstructionProbe& operator=(ConstructionProbe&&) = delete;

        static void reset() noexcept {
            construction_count = 0;
        }

        int value;
    };

    class ToggleAllocator final : public nk::mem::MallocAllocator {
    public:
        ToggleAllocator()
            : MallocAllocator{nk::mem::untracked} {}

        void reject_allocations(const bool reject) noexcept {
            m_reject = reject;
        }

    protected:
        void* _do_allocate(
            const nk::u64 size_bytes,
            const nk::u64 alignment) noexcept override {
            if (m_reject)
                return nullptr;
            return MallocAllocator::_do_allocate(size_bytes, alignment);
        }

    private:
        bool m_reject = false;
    };
}

TEST(Map, InsertsFindsAssignsRemovesAndIterates) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::map<nk::u32, nk::u64> values;
    ASSERT_TRUE(values.map_init(&allocator, 2, nk::hash_seed::deterministic));

    const auto first = values.insert(1u, 10ull);
    ASSERT_TRUE(first);
    EXPECT_EQ(first.value(), nk::cl::insert_outcome::inserted);
    const auto second = values.insert(2u, 20ull);
    ASSERT_TRUE(second);
    EXPECT_EQ(second.value(), nk::cl::insert_outcome::inserted);
    const auto duplicate = values.insert(2u, 99ull);
    ASSERT_TRUE(duplicate);
    EXPECT_EQ(duplicate.value(), nk::cl::insert_outcome::already_present);
    const auto assigned = values.insert_or_assign(2u, 25ull);
    ASSERT_TRUE(assigned);
    EXPECT_EQ(assigned.value(), nk::cl::insert_outcome::assigned);
    EXPECT_TRUE(values.contains(1u));
    ASSERT_NE(values.find(2u), nullptr);
    EXPECT_EQ(*values.find(2u), 25u);
    EXPECT_EQ(values.at(1u), 10u);

    nk::u64 total = 0;
    for (const auto entry : values)
        total += entry.value;
    EXPECT_EQ(total, 35u);

    EXPECT_TRUE(values.remove(1u));
    EXPECT_FALSE(values.remove(1u));
    EXPECT_FALSE(values.contains(1u));
    EXPECT_EQ(values.length(), 1);

    values.clear();
    EXPECT_TRUE(values.empty());
    EXPECT_GT(values.capacity(), 0);
    EXPECT_TRUE(values.map_shutdown());
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Map, HandlesLongRobinHoodClustersAndMoveOnlyValues) {
    map_test::MoveOnly::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::map<map_test::CollisionKey, map_test::MoveOnly> values;
    ASSERT_TRUE(values.map_init(&allocator, 0, 0x1234));

    for (nk::u32 index = 0; index < 300; ++index) {
        ASSERT_TRUE(values.insert(
            map_test::CollisionKey{index},
            map_test::MoveOnly{static_cast<int>(index * 3)}));
    }
    EXPECT_EQ(values.length(), 300);
    EXPECT_GE(values.capacity(), 512);

    for (nk::u32 index = 0; index < 300; ++index) {
        const auto* value = values.find(map_test::CollisionKey{index});
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(value->value, static_cast<int>(index * 3));
    }

    EXPECT_TRUE(values.remove(map_test::CollisionKey{0}));
    EXPECT_TRUE(values.remove(map_test::CollisionKey{150}));
    EXPECT_TRUE(values.remove(map_test::CollisionKey{299}));
    EXPECT_FALSE(values.contains(map_test::CollisionKey{0}));
    EXPECT_FALSE(values.contains(map_test::CollisionKey{150}));
    EXPECT_FALSE(values.contains(map_test::CollisionKey{299}));
    EXPECT_EQ(values.length(), 297);

    EXPECT_TRUE(values.map_shutdown());
    EXPECT_EQ(map_test::MoveOnly::live_count, 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Map, PreservesStateWhenGrowthAllocationFails) {
    map_test::ToggleAllocator allocator;
    nk::cl::map<nk::u32, nk::u32> values;
    ASSERT_TRUE(values.map_init(&allocator, 1, nk::hash_seed::deterministic));

    for (nk::u32 index = 0; index < 7; ++index)
        ASSERT_TRUE(values.insert(index, index + 10));
    const nk::u64 original_capacity = values.capacity();

    allocator.reject_allocations(true);
    const auto rejected = values.insert(100u, 200u);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error(), nk::cl::map_error::out_of_memory);
    EXPECT_EQ(values.length(), 7);
    EXPECT_EQ(values.capacity(), original_capacity);
    for (nk::u32 index = 0; index < 7; ++index)
        EXPECT_EQ(values.at(index), index + 10);

    allocator.reject_allocations(false);
    EXPECT_TRUE(values.map_shutdown());
}

TEST(Map, MoveTransfersFlatStorageAndSeed) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::map<nk::u32, nk::u32> source;
    nk::cl::map<nk::u32, nk::u32> destination;
    ASSERT_TRUE(source.map_init(&allocator, 4, 77));
    ASSERT_TRUE(destination.map_init(&allocator, 1, 88));
    ASSERT_TRUE(source.insert(4u, 16u));
    ASSERT_TRUE(destination.insert(1u, 1u));

    destination = std::move(source);
    EXPECT_EQ(destination.seed(), 77u);
    EXPECT_EQ(destination.at(4u), 16u);
    EXPECT_EQ(source.allocator(), nullptr);
    EXPECT_EQ(source.capacity(), 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 1);
}

TEST(Map, SupportsHeterogeneousStringLookupWithoutAllocating) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::map<nk::str, nk::u32> values;
    ASSERT_TRUE(values.map_init(&allocator, 2, nk::hash_seed::deterministic));

    auto inserted = values.try_emplace(
        nk::str{allocator, "renderer.texture.default"},
        41u);
    ASSERT_TRUE(inserted);
    EXPECT_EQ(inserted.value(), nk::cl::insert_outcome::inserted);

    const nk::u64 allocations_before = allocator.get_active_allocation_count();
    const nk::strview query{"renderer.texture.default"};
    EXPECT_TRUE(values.contains(query));
    ASSERT_NE(values.find(query), nullptr);
    EXPECT_EQ(values.at(query), 41u);
    EXPECT_EQ(allocator.get_active_allocation_count(), allocations_before);
    EXPECT_TRUE(values.remove(query));
    EXPECT_FALSE(values.contains(query));
}

TEST(Map, TryEmplaceConstructsValuesOnlyForSuccessfulInsertions) {
    map_test::ConstructionProbe::reset();
    map_test::ToggleAllocator allocator;
    nk::cl::map<nk::u32, map_test::ConstructionProbe> values;
    ASSERT_TRUE(values.map_init(&allocator, 1, nk::hash_seed::deterministic));

    auto first = values.try_emplace(7u, 20, 22);
    ASSERT_TRUE(first);
    EXPECT_EQ(first.value(), nk::cl::insert_outcome::inserted);
    EXPECT_EQ(map_test::ConstructionProbe::construction_count, 1);
    EXPECT_EQ(values.at(7u).value, 42);

    auto duplicate = values.try_emplace(7u, 100, 200);
    ASSERT_TRUE(duplicate);
    EXPECT_EQ(duplicate.value(), nk::cl::insert_outcome::already_present);
    EXPECT_EQ(map_test::ConstructionProbe::construction_count, 1);

    for (nk::u32 index = 0; index < 6; ++index)
        ASSERT_TRUE(values.try_emplace(index + 20, 1, 2));
    const int constructions_before_failure =
        map_test::ConstructionProbe::construction_count;
    allocator.reject_allocations(true);
    auto rejected = values.try_emplace(100u, 3, 4);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error(), nk::cl::map_error::out_of_memory);
    EXPECT_EQ(
        map_test::ConstructionProbe::construction_count,
        constructions_before_failure);
}

TEST(Map, ReserveReportsCapacityOverflow) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::map<nk::u32, nk::u32> values;
    ASSERT_TRUE(values.map_init(&allocator, 0, nk::hash_seed::deterministic));

    const auto reserved = values.reserve(nk::numeric::u64_max);
    ASSERT_FALSE(reserved);
    EXPECT_EQ(reserved.error(), nk::cl::map_error::capacity_overflow);
    EXPECT_EQ(values.capacity(), 0u);
}

TEST(Map, EncodesTheFullSupportedProbeDistanceWithoutTruncation) {
    constexpr nk::u64 maximum_distance =
        static_cast<nk::u64>(nk::numeric::u16_max) - 1;
    static_assert(nk::cl::map_detail::probe_distance_fits(maximum_distance));
    static_assert(!nk::cl::map_detail::probe_distance_fits(
        maximum_distance + 1));
    static_assert(
        nk::cl::map_detail::encode_probe_distance(maximum_distance) ==
        nk::numeric::u16_max);
}

TEST(Map, PreservesWraparoundClustersDuringBackwardShift) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::map<map_test::WrapKey, nk::u32> values;
    ASSERT_TRUE(values.map_init(&allocator, 6, 0));

    for (nk::u32 index = 0; index < 6; ++index)
        ASSERT_TRUE(values.insert(map_test::WrapKey{index}, index * 11));
    ASSERT_TRUE(values.remove(map_test::WrapKey{2}));

    for (nk::u32 index = 0; index < 6; ++index) {
        if (index == 2)
            continue;
        ASSERT_NE(values.find(map_test::WrapKey{index}), nullptr);
        EXPECT_EQ(values.at(map_test::WrapKey{index}), index * 11);
    }
    EXPECT_FALSE(values.contains(map_test::WrapKey{2}));
}

TEST(Map, PreservesAliasedValuesAcrossGrowth) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::map<nk::u32, nk::u64> values;
    ASSERT_TRUE(values.map_init(&allocator, 1, nk::hash_seed::deterministic));
    for (nk::u32 index = 0; index < 7; ++index)
        ASSERT_TRUE(values.insert(index, static_cast<nk::u64>(index + 100)));

    const nk::u64& aliased_value = values.at(3u);
    const nk::u32 new_key = 100;
    auto inserted = values.insert(new_key, aliased_value);
    ASSERT_TRUE(inserted);
    EXPECT_EQ(inserted.value(), nk::cl::insert_outcome::inserted);
    EXPECT_EQ(values.at(new_key), 103u);
    EXPECT_EQ(values.at(3u), 103u);
}

TEST(Map, AlignsSlotsForOveralignedValues) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::map<nk::u32, map_test::OveralignedValue> values;
    ASSERT_TRUE(values.map_init(&allocator, 4, nk::hash_seed::deterministic));
    ASSERT_TRUE(values.try_emplace(9u, 27u));

    const auto* value = values.find(9u);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(
        reinterpret_cast<std::uintptr_t>(value) %
            alignof(map_test::OveralignedValue),
        0u);
    EXPECT_EQ(value->value, 27u);
}

TEST(MapDeathTest, RejectsOperationsBeforeInitialization) {
    nk::cl::map<nk::u32, nk::u32> values;
    EXPECT_DEATH_IF_SUPPORTED(
        static_cast<void>(values.find(1u)),
        "");
}
