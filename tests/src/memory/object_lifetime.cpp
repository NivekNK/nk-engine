#include <gtest/gtest.h>

#include "collections/arr_type.h"
#include "memory/malloc_allocator.h"
#include "memory/object_lifetime.h"

namespace {
    template <typename T, std::size_t Count>
    struct RawStorage {
        alignas(T) std::byte bytes[sizeof(T) * Count];

        T* data() noexcept {
            return reinterpret_cast<T*>(bytes);
        }
    };

    struct Probe {
        static inline int live_count = 0;
        static inline int copy_count = 0;
        static inline int move_count = 0;
        static inline int move_assignment_count = 0;
        static inline int destruction_count = 0;

        int value = 17;

        Probe() noexcept {
            ++live_count;
        }

        explicit Probe(const int initial_value) noexcept
            : value{initial_value} {
            ++live_count;
        }

        Probe(const Probe& other) noexcept
            : value{other.value} {
            ++live_count;
            ++copy_count;
        }

        Probe(Probe&& other) noexcept
            : value{other.value} {
            other.value = -1;
            ++live_count;
            ++move_count;
        }

        Probe& operator=(Probe&& other) noexcept {
            value = other.value;
            other.value = -1;
            ++move_assignment_count;
            return *this;
        }

        ~Probe() noexcept {
            --live_count;
            ++destruction_count;
        }

        static void reset() noexcept {
            live_count = 0;
            copy_count = 0;
            move_count = 0;
            move_assignment_count = 0;
            destruction_count = 0;
        }
    };

    struct MoveOnly {
        int value;

        MoveOnly() = delete;

        explicit MoveOnly(const int initial_value) noexcept
            : value{initial_value} {}

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;

        MoveOnly(MoveOnly&& other) noexcept
            : value{other.value} {
            other.value = -1;
        }

        MoveOnly& operator=(MoveOnly&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }
    };

    struct alignas(256) OverAlignedMoveOnly {
        static inline int live_count = 0;

        int value;

        OverAlignedMoveOnly() = delete;

        explicit OverAlignedMoveOnly(const int initial_value) noexcept
            : value{initial_value} {
            ++live_count;
        }

        OverAlignedMoveOnly(const OverAlignedMoveOnly&) = delete;
        OverAlignedMoveOnly& operator=(const OverAlignedMoveOnly&) = delete;

        OverAlignedMoveOnly(OverAlignedMoveOnly&& other) noexcept
            : value{other.value} {
            other.value = -1;
            ++live_count;
        }

        OverAlignedMoveOnly& operator=(OverAlignedMoveOnly&&) = delete;

        ~OverAlignedMoveOnly() noexcept {
            --live_count;
        }
    };

    template <typename T>
    concept CanValueConstructRange = requires(T* data) {
        nk::mem::construct_value_range(data, 1);
    };

    template <typename T>
    concept CanCopyConstructRange = requires(T* destination, const T* source) {
        nk::mem::construct_copy_range(destination, source, 1);
    };

    template <typename T>
    concept CanMoveConstructRange = requires(T* destination, T* source) {
        nk::mem::construct_move_range(destination, source, 1);
    };

    static_assert(nk::IArrT<int>);
    static_assert(nk::IArrT<MoveOnly>);
    static_assert(nk::IArrT<OverAlignedMoveOnly>);
    static_assert(!nk::IArrT<const int>);
    static_assert(!nk::IArrT<volatile int>);
    static_assert(!nk::IArrT<int&>);
    static_assert(!nk::IArrT<int[4]>);
    static_assert(!nk::IArrT<void>);

    static_assert(!CanValueConstructRange<MoveOnly>);
    static_assert(!CanCopyConstructRange<MoveOnly>);
    static_assert(CanMoveConstructRange<MoveOnly>);
    static_assert(nk::mem::RelocatableObject<MoveOnly>);
    static_assert(nk::mem::RelocatableObject<OverAlignedMoveOnly>);
    static_assert(nk::mem::TriviallyRelocatable<int>);
    static_assert(!nk::mem::TriviallyRelocatable<Probe>);
}

TEST(ObjectLifetime, AppliesRequirementsToEachOperation) {
    EXPECT_FALSE(CanValueConstructRange<MoveOnly>);
    EXPECT_FALSE(CanCopyConstructRange<MoveOnly>);
    EXPECT_TRUE(CanMoveConstructRange<MoveOnly>);
    EXPECT_TRUE(nk::mem::RelocatableObject<MoveOnly>);
}

TEST(ObjectLifetime, ValueAndCopyConstructsThenDestroysNonTrivialRanges) {
    Probe::reset();
    RawStorage<Probe, 3> source_storage;
    RawStorage<Probe, 3> destination_storage;
    Probe* source = source_storage.data();
    Probe* destination = destination_storage.data();

    nk::mem::construct_value_range(source, 3);
    ASSERT_EQ(Probe::live_count, 3);
    for (std::size_t index = 0; index < 3; ++index)
        EXPECT_EQ(source[index].value, 17);

    nk::mem::construct_copy_range(destination, source, 3);
    EXPECT_EQ(Probe::live_count, 6);
    EXPECT_EQ(Probe::copy_count, 3);
    for (std::size_t index = 0; index < 3; ++index)
        EXPECT_EQ(destination[index].value, 17);

    nk::mem::destroy_range(destination, 3);
    nk::mem::destroy_range(source, 3);
    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(Probe::destruction_count, 6);
}

TEST(ObjectLifetime, MoveConstructsAndMoveAssignsMoveOnlyRanges) {
    RawStorage<MoveOnly, 3> source_storage;
    RawStorage<MoveOnly, 3> destination_storage;
    MoveOnly* source = source_storage.data();
    MoveOnly* destination = destination_storage.data();

    for (std::size_t index = 0; index < 3; ++index) {
        (void)nk::mem::construct_object(
            source + index,
            static_cast<int>(index + 10));
    }

    nk::mem::construct_move_range(destination, source, 3);
    for (std::size_t index = 0; index < 3; ++index) {
        EXPECT_EQ(source[index].value, -1);
        EXPECT_EQ(destination[index].value, static_cast<int>(index + 10));
    }

    nk::mem::move_assign_range(source, destination, 3);
    for (std::size_t index = 0; index < 3; ++index) {
        EXPECT_EQ(source[index].value, static_cast<int>(index + 10));
        EXPECT_EQ(destination[index].value, -1);
    }

    nk::mem::destroy_range(destination, 3);
    nk::mem::destroy_range(source, 3);
}

TEST(ObjectLifetime, MoveAssignmentPreservesOverlappingRangeOrder) {
    Probe::reset();
    RawStorage<Probe, 5> right_storage;
    Probe* right = right_storage.data();

    for (std::size_t index = 0; index < 5; ++index)
        (void)nk::mem::construct_object(right + index, static_cast<int>(index));

    nk::mem::move_assign_range(right + 1, right, 4);

    EXPECT_EQ(right[0].value, -1);
    for (std::size_t index = 1; index < 5; ++index)
        EXPECT_EQ(right[index].value, static_cast<int>(index - 1));
    EXPECT_EQ(Probe::move_assignment_count, 4);

    nk::mem::destroy_range(right, 5);
    EXPECT_EQ(Probe::live_count, 0);

    Probe::reset();
    RawStorage<Probe, 5> left_storage;
    Probe* left = left_storage.data();
    for (std::size_t index = 0; index < 5; ++index)
        (void)nk::mem::construct_object(left + index, static_cast<int>(index));

    nk::mem::move_assign_range(left, left + 1, 4);

    for (std::size_t index = 0; index < 4; ++index)
        EXPECT_EQ(left[index].value, static_cast<int>(index + 1));
    EXPECT_EQ(left[4].value, -1);
    EXPECT_EQ(Probe::move_assignment_count, 4);

    nk::mem::destroy_range(left, 5);
    EXPECT_EQ(Probe::live_count, 0);
}

TEST(ObjectLifetime, RelocatesNonTrivialObjectsAndEndsSourceLifetimes) {
    Probe::reset();
    RawStorage<Probe, 3> source_storage;
    RawStorage<Probe, 3> destination_storage;
    Probe* source = source_storage.data();
    Probe* destination = destination_storage.data();

    for (std::size_t index = 0; index < 3; ++index)
        (void)nk::mem::construct_object(source + index, static_cast<int>(index));

    nk::mem::relocate_range(destination, source, 3);

    EXPECT_EQ(Probe::live_count, 3);
    EXPECT_EQ(Probe::move_count, 3);
    EXPECT_EQ(Probe::destruction_count, 3);
    for (std::size_t index = 0; index < 3; ++index)
        EXPECT_EQ(destination[index].value, static_cast<int>(index));

    nk::mem::destroy_range(destination, 3);
    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(Probe::destruction_count, 6);
}

TEST(ObjectLifetime, RelocatesOverlappingNonTrivialRangesInBothDirections) {
    Probe::reset();
    RawStorage<Probe, 5> right_storage;
    Probe* right = right_storage.data();
    for (std::size_t index = 0; index < 4; ++index)
        (void)nk::mem::construct_object(right + index, static_cast<int>(index));

    nk::mem::relocate_range(right + 1, right, 4);
    EXPECT_EQ(Probe::live_count, 4);
    for (std::size_t index = 1; index < 5; ++index)
        EXPECT_EQ(right[index].value, static_cast<int>(index - 1));
    nk::mem::destroy_range(right + 1, 4);
    EXPECT_EQ(Probe::live_count, 0);

    Probe::reset();
    RawStorage<Probe, 5> left_storage;
    Probe* left = left_storage.data();
    for (std::size_t index = 1; index < 5; ++index) {
        (void)nk::mem::construct_object(
            left + index,
            static_cast<int>(index + 20));
    }

    nk::mem::relocate_range(left, left + 1, 4);
    EXPECT_EQ(Probe::live_count, 4);
    for (std::size_t index = 0; index < 4; ++index)
        EXPECT_EQ(left[index].value, static_cast<int>(index + 21));
    nk::mem::destroy_range(left, 4);
    EXPECT_EQ(Probe::live_count, 0);
}

TEST(ObjectLifetime, UsesBytewiseOperationsForTrivialRanges) {
    RawStorage<int, 4> source_storage;
    RawStorage<int, 4> destination_storage;
    int* source = source_storage.data();
    int* destination = destination_storage.data();

    nk::mem::construct_value_range(source, 4);
    for (std::size_t index = 0; index < 4; ++index)
        source[index] = static_cast<int>(index + 1);

    nk::mem::construct_copy_range(destination, source, 4);
    for (std::size_t index = 0; index < 4; ++index)
        EXPECT_EQ(destination[index], static_cast<int>(index + 1));

    nk::mem::move_assign_range(destination + 1, destination, 3);
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination[1], 1);
    EXPECT_EQ(destination[2], 2);
    EXPECT_EQ(destination[3], 3);

    nk::mem::destroy_range(destination, 4);
    nk::mem::destroy_range(source, 4);
}

TEST(ObjectLifetime, PreservesAlignmentWhileRelocatingOverAlignedMoveOnlyObjects) {
    OverAlignedMoveOnly::live_count = 0;
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    constexpr std::size_t count = 2;

    OverAlignedMoveOnly* source =
        allocator._allocate_lot_t<OverAlignedMoveOnly>(count);
    OverAlignedMoveOnly* destination =
        allocator._allocate_lot_t<OverAlignedMoveOnly>(count);
    ASSERT_NE(source, nullptr);
    ASSERT_NE(destination, nullptr);
    ASSERT_EQ(
        reinterpret_cast<std::uintptr_t>(source) % alignof(OverAlignedMoveOnly),
        0);
    ASSERT_EQ(
        reinterpret_cast<std::uintptr_t>(destination) %
            alignof(OverAlignedMoveOnly),
        0);

    for (std::size_t index = 0; index < count; ++index) {
        (void)nk::mem::construct_object(
            source + index,
            static_cast<int>(index + 40));
    }
    ASSERT_EQ(OverAlignedMoveOnly::live_count, 2);

    nk::mem::relocate_range(destination, source, count);
    EXPECT_EQ(OverAlignedMoveOnly::live_count, 2);
    EXPECT_EQ(destination[0].value, 40);
    EXPECT_EQ(destination[1].value, 41);

    nk::mem::destroy_range(destination, count);
    EXPECT_EQ(OverAlignedMoveOnly::live_count, 0);
    EXPECT_TRUE(allocator._free_lot_t(source, count));
    EXPECT_TRUE(allocator._free_lot_t(destination, count));
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(ObjectLifetime, AcceptsEmptyRangesWithoutStorage) {
    nk::mem::construct_value_range<int>(nullptr, 0);
    nk::mem::construct_copy_range<int>(nullptr, nullptr, 0);
    nk::mem::construct_move_range<int>(nullptr, nullptr, 0);
    nk::mem::move_assign_range<int>(nullptr, nullptr, 0);
    nk::mem::relocate_range<int>(nullptr, nullptr, 0);
    nk::mem::destroy_range<int>(nullptr, 0);
}
