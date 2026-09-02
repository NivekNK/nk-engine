#include <cstdint>

#include <gtest/gtest.h>

#include "collections/arr.h"
#include "collections/dyarr.h"
#include "memory/malloc_allocator.h"

namespace {
    struct Probe {
        static inline int live_count = 0;
        static inline int default_count = 0;
        static inline int copy_count = 0;
        static inline int move_count = 0;
        static inline int destruction_count = 0;

        int value = 17;

        Probe() noexcept {
            ++live_count;
            ++default_count;
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

        Probe& operator=(const Probe&) = delete;
        Probe& operator=(Probe&&) = delete;

        ~Probe() noexcept {
            --live_count;
            ++destruction_count;
        }

        static void reset() noexcept {
            live_count = 0;
            default_count = 0;
            copy_count = 0;
            move_count = 0;
            destruction_count = 0;
        }
    };

    struct NoDefault {
        explicit NoDefault(const int initial_value) noexcept
            : value{initial_value} {}

        NoDefault(NoDefault&&) noexcept = default;
        NoDefault& operator=(NoDefault&&) = delete;

        int value;
    };

    struct alignas(256) AlignedMoveOnly {
        int value = 31;

        AlignedMoveOnly() noexcept = default;
        AlignedMoveOnly(const AlignedMoveOnly&) = delete;
        AlignedMoveOnly& operator=(const AlignedMoveOnly&) = delete;

        AlignedMoveOnly(AlignedMoveOnly&& other) noexcept
            : value{other.value} {
            other.value = -1;
        }

        AlignedMoveOnly& operator=(AlignedMoveOnly&&) = delete;
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

        bool _do_free(void*, nk::u64) noexcept override {
            return false;
        }
    };

    class ToggleAllocator final : public nk::mem::MallocAllocator {
    public:
        ToggleAllocator()
            : MallocAllocator{nk::mem::untracked} {}

        void reject_allocations(const bool reject) noexcept {
            m_reject_allocations = reject;
        }

    protected:
        void* _do_allocate(
            const nk::u64 size_bytes,
            const nk::u64 alignment) noexcept override {
            if (m_reject_allocations)
                return nullptr;
            return MallocAllocator::_do_allocate(size_bytes, alignment);
        }

        bool _do_free(
            void* data,
            const nk::u64 size_bytes) noexcept override {
            return MallocAllocator::_do_free(data, size_bytes);
        }

    private:
        bool m_reject_allocations = false;
    };

    class OwnedAllocatorProbe final : public nk::mem::MallocAllocator {
    public:
        static inline int destruction_count = 0;

        explicit OwnedAllocatorProbe(const nk::mem::Untracked tag)
            : MallocAllocator{tag} {}

        ~OwnedAllocatorProbe() override {
            ++destruction_count;
        }
    };

    template <typename T>
    concept CanInitializeReserved = requires(
        nk::cl::dyarr<T>& values,
        nk::mem::Allocator* allocator) {
        { values._dyarr_init(allocator, 4) } -> std::same_as<bool>;
    };

    template <typename T>
    concept CanInitializeLength = requires(
        nk::cl::dyarr<T>& values,
        nk::mem::Allocator* allocator) {
        { values._dyarr_init_len(allocator, 4, 2) } -> std::same_as<bool>;
    };

    static_assert(CanInitializeReserved<NoDefault>);
    static_assert(!CanInitializeLength<NoDefault>);
    static_assert(
        sizeof(nk::cl::dyarr<nk::u64>) ==
        sizeof(nk::u64*) +
        sizeof(nk::u64) * 2 +
        sizeof(nk::mem::Allocator*) +
        sizeof(nk::mem::AllocatorOwner::Destroy));
}

TEST(Dyarr, ReservesStorageWithoutConstructingElements) {
    Probe::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<Probe> array;

    ASSERT_TRUE(array.dyarr_init(&allocator, 0));
    EXPECT_EQ(array.length(), 0);
    EXPECT_EQ(array.capacity(), 4);
    EXPECT_NE(array.data(), nullptr);
    EXPECT_EQ(array.begin(), array.end());
    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 1);

    EXPECT_TRUE(array.dyarr_clear());
    EXPECT_EQ(array.data(), nullptr);
    EXPECT_EQ(array.capacity(), 0);
    EXPECT_EQ(array.allocator(), &allocator);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);

    ASSERT_TRUE(array._dyarr_init(&allocator, 2));
    EXPECT_EQ(array.capacity(), 4);
    EXPECT_TRUE(array.dyarr_shutdown());
    EXPECT_EQ(array.allocator(), nullptr);
}

TEST(Dyarr, ValueInitializesAndDestroysOnlyTheLiveRange) {
    Probe::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<Probe> array;

    ASSERT_TRUE(array.dyarr_init_len(&allocator, 2, 2));
    EXPECT_EQ(array.capacity(), 4);
    EXPECT_EQ(Probe::live_count, 2);
    EXPECT_EQ(Probe::default_count, 2);

    Probe* original_data = array.data();
    ASSERT_TRUE(array.dyarr_resize(4));
    EXPECT_EQ(array.data(), original_data);
    EXPECT_EQ(Probe::live_count, 4);
    EXPECT_EQ(Probe::default_count, 4);

    ASSERT_TRUE(array.dyarr_resize(6));
    EXPECT_EQ(array.capacity(), 8);
    EXPECT_EQ(array.length(), 6);
    EXPECT_EQ(Probe::live_count, 6);
    EXPECT_EQ(Probe::move_count, 4);
    EXPECT_EQ(Probe::default_count, 6);

    ASSERT_TRUE(array.dyarr_resize(2));
    EXPECT_EQ(Probe::live_count, 2);

    Probe* reserved_data = array.data();
    const nk::u64 reserved_capacity = array.capacity();
    EXPECT_TRUE(array.dyarr_reset());
    EXPECT_EQ(array.length(), 0);
    EXPECT_EQ(array.data(), reserved_data);
    EXPECT_EQ(array.capacity(), reserved_capacity);
    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 1);

    ASSERT_TRUE(array.dyarr_resize(3));
    EXPECT_EQ(array.data(), reserved_data);
    EXPECT_EQ(Probe::live_count, 3);
    EXPECT_TRUE(array.dyarr_shutdown());
    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Dyarr, CopiesInitializerListsAndNonTrivialInsertions) {
    Probe::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<Probe> array;

    ASSERT_TRUE(array.dyarr_init_list(
        &allocator,
        {Probe{2}, Probe{4}}));
    EXPECT_EQ(array.length(), 2);
    EXPECT_EQ(array[0].value, 2);
    EXPECT_EQ(array[1].value, 4);
    EXPECT_EQ(Probe::copy_count, 2);
    EXPECT_EQ(Probe::move_count, 0);
    EXPECT_EQ(Probe::live_count, 2);

    Probe copied{6};
    ASSERT_TRUE(array.dyarr_insert_copy(1, copied));
    EXPECT_EQ(copied.value, 6);
    EXPECT_EQ(array.length(), 3);
    EXPECT_EQ(array[0].value, 2);
    EXPECT_EQ(array[1].value, 6);
    EXPECT_EQ(array[2].value, 4);
    EXPECT_EQ(Probe::copy_count, 3);

    EXPECT_TRUE(array.dyarr_shutdown());
    EXPECT_EQ(Probe::live_count, 1);
}

TEST(Dyarr, DoesNotAllocateForMutationsWithinCapacity) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<nk::u32> array;
    ASSERT_TRUE(array.dyarr_init(&allocator, 8));
    nk::u32* reserved_data = array.data();

    for (nk::u32 value : {1u, 2u, 3u})
        ASSERT_TRUE(array.dyarr_push_copy(value));
    ASSERT_TRUE(array.dyarr_insert_copy(1, 9u));
    ASSERT_TRUE(array.dyarr_resize(7));

    EXPECT_EQ(array.data(), reserved_data);
    EXPECT_EQ(array.capacity(), 8);
    EXPECT_EQ(allocator.get_active_allocation_count(), 1);
    EXPECT_EQ(array[0], 1u);
    EXPECT_EQ(array[1], 9u);
    EXPECT_EQ(array[2], 2u);
    EXPECT_EQ(array[3], 3u);
    EXPECT_EQ(array[4], 0u);
    EXPECT_EQ(array[5], 0u);
    EXPECT_EQ(array[6], 0u);

    array.dyarr_at(7) = 11u;
    EXPECT_EQ(array.data(), reserved_data);
    EXPECT_EQ(array.length(), 8);
    EXPECT_EQ(array.dyarr_last(), 11u);

    nk::u32 sum = 0;
    for (const nk::u32 value : array)
        sum += value;
    EXPECT_EQ(sum, 26u);

    EXPECT_TRUE(array.dyarr_shutdown());
}

TEST(Dyarr, GrowsForFarInsertionAndValueInitializesEveryGap) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<nk::u32> array;
    ASSERT_TRUE(array.dyarr_init_list(&allocator, {1u, 2u, 3u}));

    ASSERT_TRUE(array.dyarr_insert_copy(8, 12u));
    ASSERT_EQ(array.length(), 9);
    EXPECT_GE(array.capacity(), 9);
    EXPECT_EQ(array[0], 1u);
    EXPECT_EQ(array[1], 2u);
    EXPECT_EQ(array[2], 3u);
    for (nk::u64 index = 3; index < 8; ++index)
        EXPECT_EQ(array[index], 0u);
    EXPECT_EQ(array[8], 12u);

    array.dyarr_at(12) = 21u;
    ASSERT_EQ(array.length(), 13);
    for (nk::u64 index = 9; index < 12; ++index)
        EXPECT_EQ(array[index], 0u);
    EXPECT_EQ(array[12], 21u);

    EXPECT_TRUE(array.dyarr_shutdown());
}

TEST(Dyarr, InsertsRemovesAndPopsWithoutMoveAssignment) {
    Probe::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<Probe> array;
    ASSERT_TRUE(array._dyarr_init_len(&allocator, 4, 3));
    array[0].value = 1;
    array[1].value = 2;
    array[2].value = 3;

    Probe inserted{9};
    ASSERT_TRUE(array.dyarr_insert(1, inserted));
    EXPECT_EQ(inserted.value, -1);
    ASSERT_EQ(array.length(), 4);
    EXPECT_EQ(array[0].value, 1);
    EXPECT_EQ(array[1].value, 9);
    EXPECT_EQ(array[2].value, 2);
    EXPECT_EQ(array[3].value, 3);

    std::optional<Probe> removed = array.dyarr_remove(2);
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(removed->value, 2);
    ASSERT_EQ(array.length(), 3);
    EXPECT_EQ(array[0].value, 1);
    EXPECT_EQ(array[1].value, 9);
    EXPECT_EQ(array[2].value, 3);

    std::optional<Probe> popped = array.dyarr_pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(popped->value, 3);
    EXPECT_EQ(array.length(), 2);
    EXPECT_FALSE(array.dyarr_remove(20).has_value());

    removed.reset();
    popped.reset();
    EXPECT_TRUE(array.dyarr_shutdown());
    EXPECT_EQ(Probe::live_count, 1);
}

TEST(Dyarr, PreservesAliasedValuesAcrossGrowthAndInsertion) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<nk::u32> array;
    ASSERT_TRUE(array._dyarr_init_list(&allocator, {1u, 2u, 3u, 4u}));
    ASSERT_EQ(array.capacity(), 4);

    ASSERT_TRUE(array._dyarr_push_copy(array[1]));
    ASSERT_EQ(array.length(), 5);
    EXPECT_EQ(array[4], 2u);

    ASSERT_TRUE(array._dyarr_insert_copy(0, array[4]));
    ASSERT_EQ(array.length(), 6);
    EXPECT_EQ(array[0], 2u);
    EXPECT_EQ(array[1], 1u);
    EXPECT_EQ(array[2], 2u);
    EXPECT_EQ(array[3], 3u);
    EXPECT_EQ(array[4], 4u);
    EXPECT_EQ(array[5], 2u);

    EXPECT_TRUE(array._dyarr_shutdown());
}

TEST(Dyarr, KeepsStateUnchangedWhenAllocationFails) {
    RejectingAllocator rejecting_allocator;
    nk::cl::dyarr<nk::u32> rejected;
    EXPECT_FALSE(rejected._dyarr_init(&rejecting_allocator, 4));
    EXPECT_EQ(rejected.data(), nullptr);
    EXPECT_EQ(rejected.length(), 0);
    EXPECT_EQ(rejected.capacity(), 0);
    EXPECT_EQ(rejected.allocator(), nullptr);

    ToggleAllocator allocator;
    nk::cl::dyarr<nk::u32> array;
    ASSERT_TRUE(array._dyarr_init_list(&allocator, {1u, 2u, 3u, 4u}));
    nk::u32* original_data = array.data();
    const nk::u64 original_capacity = array.capacity();

    allocator.reject_allocations(true);
    EXPECT_FALSE(array._dyarr_push_copy(5u));
    EXPECT_FALSE(array._dyarr_resize(9));
    EXPECT_FALSE(array._dyarr_insert_copy(12, 7u));
    EXPECT_EQ(array.data(), original_data);
    EXPECT_EQ(array.length(), 4);
    EXPECT_EQ(array.capacity(), original_capacity);
    EXPECT_EQ(array[0], 1u);
    EXPECT_EQ(array[3], 4u);

    allocator.reject_allocations(false);
    EXPECT_TRUE(array._dyarr_shutdown());

    auto rejected_owner =
        nk::mem::AllocatorOwner::make_native_untracked<ToggleAllocator>();
    ASSERT_TRUE(rejected_owner);
    static_cast<ToggleAllocator*>(rejected_owner.get())
        ->reject_allocations(true);
    nk::cl::dyarr<nk::u32> rejected_owned;
    EXPECT_FALSE(rejected_owned._dyarr_init_own(
        std::move(rejected_owner),
        4));
    EXPECT_TRUE(rejected_owner);
    EXPECT_EQ(rejected_owned.allocator(), nullptr);
    rejected_owner.reset();
}

TEST(Dyarr, MoveOperationsReleaseDestinationAndHandleSelfMove) {
    Probe::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    {
        nk::cl::dyarr<Probe> source;
        nk::cl::dyarr<Probe> destination;
        ASSERT_TRUE(source._dyarr_init_len(&allocator, 4, 4));
        ASSERT_TRUE(destination._dyarr_init_len(&allocator, 2, 2));
        Probe* source_data = source.data();
        EXPECT_EQ(allocator.get_active_allocation_count(), 2);

        destination = std::move(source);
        EXPECT_EQ(destination.data(), source_data);
        EXPECT_EQ(destination.length(), 4);
        EXPECT_EQ(source.data(), nullptr);
        EXPECT_EQ(source.length(), 0);
        EXPECT_EQ(source.capacity(), 0);
        EXPECT_EQ(source.allocator(), nullptr);
        EXPECT_EQ(Probe::live_count, 4);
        EXPECT_EQ(allocator.get_active_allocation_count(), 1);

        nk::cl::dyarr<Probe>* destination_pointer = &destination;
        destination = std::move(*destination_pointer);
        EXPECT_EQ(destination.data(), source_data);
        EXPECT_EQ(destination.length(), 4);
        EXPECT_EQ(Probe::live_count, 4);

        nk::cl::dyarr<Probe> final{std::move(destination)};
        EXPECT_EQ(final.data(), source_data);
        EXPECT_EQ(destination.data(), nullptr);
    }

    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Dyarr, MoveAssignmentTransfersTypedOwnership) {
    OwnedAllocatorProbe::destruction_count = 0;
    auto source_owner =
        nk::mem::AllocatorOwner::make_native_untracked<
            OwnedAllocatorProbe>(nk::mem::untracked);
    auto destination_owner =
        nk::mem::AllocatorOwner::make_native_untracked<
            OwnedAllocatorProbe>(nk::mem::untracked);
    ASSERT_TRUE(source_owner);
    ASSERT_TRUE(destination_owner);

    nk::cl::dyarr<nk::u32> source;
    nk::cl::dyarr<nk::u32> destination;
    ASSERT_TRUE(source.dyarr_init_own_len(
        std::move(source_owner),
        4,
        2));
    ASSERT_TRUE(destination.dyarr_init_own(
        std::move(destination_owner),
        4));

    destination = std::move(source);
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 1);
    EXPECT_TRUE(destination.owns_allocator());
    EXPECT_FALSE(source.owns_allocator());
    EXPECT_EQ(source.allocator(), nullptr);

    nk::cl::dyarr<nk::u32>* destination_pointer = &destination;
    destination = std::move(*destination_pointer);
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 1);
    EXPECT_TRUE(destination.owns_allocator());

    EXPECT_TRUE(destination._dyarr_shutdown());
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 2);
}

TEST(Dyarr, OwnsAllocatorThroughTypedToken) {
    OwnedAllocatorProbe::destruction_count = 0;
    auto owner = nk::mem::AllocatorOwner::make_native_untracked<
        OwnedAllocatorProbe>(nk::mem::untracked);
    ASSERT_TRUE(owner);
    nk::mem::Allocator* allocator = owner.get();

    nk::cl::dyarr<nk::u32> array;
    ASSERT_TRUE(array.dyarr_init_list_own(
        std::move(owner),
        {3u, 5u, 8u}));
    EXPECT_FALSE(owner);
    EXPECT_TRUE(array.owns_allocator());
    EXPECT_EQ(array.allocator(), allocator);

    EXPECT_TRUE(array.dyarr_clear());
    EXPECT_TRUE(array.owns_allocator());
    EXPECT_EQ(array.allocator(), allocator);
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 0);

    ASSERT_TRUE(array._dyarr_init_len(array.allocator(), 2, 2));
    EXPECT_TRUE(array.dyarr_shutdown());
    EXPECT_FALSE(array.owns_allocator());
    EXPECT_EQ(array.allocator(), nullptr);
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 1);

}

TEST(Dyarr, TransfersTypedAllocatorOwnershipToFixedArray) {
    OwnedAllocatorProbe::destruction_count = 0;
    auto owner = nk::mem::AllocatorOwner::make_native_untracked<
        OwnedAllocatorProbe>(nk::mem::untracked);
    ASSERT_TRUE(owner);

    nk::cl::dyarr<nk::u32> dynamic;
    ASSERT_TRUE(dynamic._dyarr_init_list_own(
        std::move(owner),
        {2u, 4u, 8u}));
    nk::cl::arr<nk::u32> fixed{std::move(dynamic)};

    EXPECT_TRUE(fixed.owns_allocator());
    EXPECT_EQ(fixed.length(), 3);
    EXPECT_EQ(fixed[0], 2u);
    EXPECT_EQ(fixed[1], 4u);
    EXPECT_EQ(fixed[2], 8u);
    EXPECT_EQ(dynamic.data(), nullptr);
    EXPECT_EQ(dynamic.allocator(), nullptr);
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 0);

    EXPECT_TRUE(fixed._arr_shutdown());
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 1);
}

TEST(Dyarr, SupportsPointerHelpersAndOveralignedMoveOnlyTypes) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::dyarr<nk::cstr> pointers;
    ASSERT_TRUE(pointers.dyarr_init(&allocator, 0));
    ASSERT_TRUE(pointers.dyarr_push_ptr("alpha"));
    ASSERT_TRUE(pointers.dyarr_insert_ptr(3, "omega"));
    EXPECT_STREQ(pointers[0], "alpha");
    EXPECT_EQ(pointers[1], nullptr);
    EXPECT_EQ(pointers[2], nullptr);
    EXPECT_STREQ(pointers[3], "omega");
    EXPECT_TRUE(pointers.dyarr_shutdown());

    nk::cl::dyarr<AlignedMoveOnly> aligned;
    ASSERT_TRUE(aligned._dyarr_init_len(&allocator, 0, 5));
    EXPECT_EQ(
        reinterpret_cast<std::uintptr_t>(aligned.data()) %
            alignof(AlignedMoveOnly),
        0u);
    ASSERT_TRUE(aligned._dyarr_resize(10));
    EXPECT_EQ(
        reinterpret_cast<std::uintptr_t>(aligned.data()) %
            alignof(AlignedMoveOnly),
        0u);
    EXPECT_TRUE(aligned._dyarr_shutdown());
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}
