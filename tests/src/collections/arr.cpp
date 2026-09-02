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

        Probe& operator=(Probe&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }

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

    struct DefaultMoveOnly {
        static inline int live_count = 0;

        int value = 23;

        DefaultMoveOnly() noexcept {
            ++live_count;
        }

        DefaultMoveOnly(const DefaultMoveOnly&) = delete;
        DefaultMoveOnly& operator=(const DefaultMoveOnly&) = delete;

        DefaultMoveOnly(DefaultMoveOnly&& other) noexcept
            : value{other.value} {
            other.value = -1;
            ++live_count;
        }

        DefaultMoveOnly& operator=(DefaultMoveOnly&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }

        ~DefaultMoveOnly() noexcept {
            --live_count;
        }
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

    class OwnedAllocatorProbe final : public nk::mem::MallocAllocator {
    public:
        static inline int destruction_count = 0;

        OwnedAllocatorProbe() = default;

        explicit OwnedAllocatorProbe(const nk::mem::Untracked tag)
            : MallocAllocator{tag} {}

        ~OwnedAllocatorProbe() override {
            ++destruction_count;
        }
    };

    template <typename T>
    concept CanInitializeLength = requires(
        nk::cl::arr<T>& values,
        nk::mem::Allocator* allocator) {
        { values._arr_init(allocator, 1) } -> std::same_as<bool>;
    };

    template <typename T>
    concept CanInitializeList = requires(
        nk::cl::arr<T>& values,
        nk::mem::Allocator* allocator,
        std::initializer_list<T> list) {
        { values._arr_init_list(allocator, list) } -> std::same_as<bool>;
    };

    static_assert(CanInitializeLength<DefaultMoveOnly>);
    static_assert(!CanInitializeList<DefaultMoveOnly>);
    static_assert(
        sizeof(nk::cl::arr<nk::u64>) ==
        sizeof(nk::u64*) +
        sizeof(nk::u64) +
        sizeof(nk::mem::Allocator*) +
        sizeof(nk::mem::AllocatorOwner::Destroy));
}

TEST(Arr, ValueInitializesScalarsAndSupportsIteration) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::arr<nk::u32> array;

    ASSERT_TRUE(array.arr_init(&allocator, 10));
    ASSERT_EQ(array.length(), 10);
    ASSERT_EQ(allocator.get_active_allocation_count(), 1);

    nk::u32 next = 1;
    for (nk::u32& value : array) {
        EXPECT_EQ(value, 0u);
        value = next++;
    }

    const nk::cl::arr<nk::u32>& const_array = array;
    EXPECT_EQ(const_array.first(), 1u);
    EXPECT_EQ(const_array.last(), 10u);
    EXPECT_EQ(const_array.begin(), array.data());
    EXPECT_EQ(const_array.end(), array.data() + array.length());

    EXPECT_TRUE(array.arr_clear());
    EXPECT_TRUE(array.empty());
    EXPECT_EQ(array.data(), nullptr);
    EXPECT_EQ(array.allocator(), &allocator);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);

    EXPECT_TRUE(array.arr_shutdown());
    EXPECT_EQ(array.allocator(), nullptr);
}

TEST(Arr, ConstructsAndDestroysEveryNonTrivialElementExactlyOnce) {
    Probe::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    {
        nk::cl::arr<Probe> array;
        ASSERT_TRUE(array._arr_init(&allocator, 4));
        EXPECT_EQ(Probe::live_count, 4);
        EXPECT_EQ(Probe::default_count, 4);
        EXPECT_EQ(Probe::destruction_count, 0);
        EXPECT_EQ(allocator.get_active_allocation_count(), 1);
    }

    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(Probe::destruction_count, 4);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Arr, CopiesInitializerListElementsIntoOwnedStorage) {
    Probe::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::arr<Probe> array;

    ASSERT_TRUE(array.arr_init_list(
        &allocator,
        {Probe{3}, Probe{5}, Probe{8}}));

    EXPECT_EQ(array.length(), 3);
    EXPECT_EQ(array[0].value, 3);
    EXPECT_EQ(array[1].value, 5);
    EXPECT_EQ(array[2].value, 8);
    EXPECT_EQ(Probe::copy_count, 3);
    EXPECT_EQ(Probe::move_count, 0);
    EXPECT_EQ(Probe::live_count, 3);
    EXPECT_EQ(Probe::destruction_count, 3);

    EXPECT_TRUE(array.arr_shutdown());
    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(Probe::destruction_count, 6);
}

TEST(Arr, SupportsDefaultConstructibleMoveOnlyElements) {
    DefaultMoveOnly::live_count = 0;
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    {
        nk::cl::arr<DefaultMoveOnly> array;
        ASSERT_TRUE(array._arr_init(&allocator, 3));
        EXPECT_EQ(DefaultMoveOnly::live_count, 3);
        for (const DefaultMoveOnly& value : array)
            EXPECT_EQ(value.value, 23);
    }

    EXPECT_EQ(DefaultMoveOnly::live_count, 0);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Arr, MoveOperationsTransferStorageAndReleaseTheDestination) {
    Probe::reset();
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    {
        nk::cl::arr<Probe> source;
        ASSERT_TRUE(source._arr_init(&allocator, 3));
        Probe* source_data = source.data();

        nk::cl::arr<Probe> middle{std::move(source)};
        EXPECT_EQ(middle.data(), source_data);
        EXPECT_EQ(middle.length(), 3);
        EXPECT_EQ(source.data(), nullptr);
        EXPECT_EQ(source.length(), 0);
        EXPECT_EQ(source.allocator(), nullptr);
        EXPECT_EQ(Probe::move_count, 0);

        nk::cl::arr<Probe> destination;
        ASSERT_TRUE(destination._arr_init(&allocator, 2));
        EXPECT_EQ(Probe::live_count, 5);
        EXPECT_EQ(allocator.get_active_allocation_count(), 2);

        destination = std::move(middle);
        EXPECT_EQ(Probe::live_count, 3);
        EXPECT_EQ(Probe::destruction_count, 2);
        EXPECT_EQ(destination.data(), source_data);
        EXPECT_EQ(middle.data(), nullptr);
        EXPECT_EQ(middle.allocator(), nullptr);
        EXPECT_EQ(allocator.get_active_allocation_count(), 1);

        nk::cl::arr<Probe>* destination_pointer = &destination;
        destination = std::move(*destination_pointer);
        EXPECT_EQ(destination.data(), source_data);
        EXPECT_EQ(destination.length(), 3);
        EXPECT_EQ(Probe::live_count, 3);
    }

    EXPECT_EQ(Probe::live_count, 0);
    EXPECT_EQ(Probe::destruction_count, 5);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Arr, RejectsFailedOrRepeatedInitializationTransactionally) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    RejectingAllocator rejecting_allocator;
    nk::cl::arr<nk::u32> array;

    EXPECT_FALSE(array._arr_init(&rejecting_allocator, 4));
    EXPECT_EQ(array.data(), nullptr);
    EXPECT_EQ(array.length(), 0);
    EXPECT_EQ(array.allocator(), nullptr);

    auto rejected_owner =
        nk::mem::AllocatorOwner::make_native_untracked<RejectingAllocator>();
    ASSERT_TRUE(rejected_owner);
    nk::cl::arr<nk::u32> rejected_owned_array;
    EXPECT_FALSE(rejected_owned_array._arr_init_own(
        std::move(rejected_owner),
        4));
    EXPECT_TRUE(rejected_owner);
    EXPECT_EQ(rejected_owned_array.data(), nullptr);
    EXPECT_EQ(rejected_owned_array.allocator(), nullptr);
    rejected_owner.reset();

    ASSERT_TRUE(array._arr_init(&allocator, 2));
    nk::u32* original_data = array.data();
    EXPECT_FALSE(array._arr_init(&allocator, 3));
    EXPECT_EQ(array.data(), original_data);
    EXPECT_EQ(array.length(), 2);
    EXPECT_EQ(array.allocator(), &allocator);

    ASSERT_TRUE(array._arr_clear());
    EXPECT_FALSE(array._arr_init(&rejecting_allocator, 4));
    EXPECT_EQ(array.data(), nullptr);
    EXPECT_EQ(array.length(), 0);
    EXPECT_EQ(array.allocator(), &allocator);

    EXPECT_TRUE(array._arr_init(&allocator, 1));
    EXPECT_TRUE(array.arr_reset());
    EXPECT_EQ(array.data(), nullptr);
    EXPECT_EQ(array.allocator(), nullptr);
}

TEST(Arr, EmptyArraysRetainAndThenDisconnectTheirBorrowedAllocator) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::cl::arr<nk::u64> array;

    ASSERT_TRUE(array._arr_init(&allocator, 0));
    EXPECT_TRUE(array.empty());
    EXPECT_EQ(array.data(), nullptr);
    EXPECT_EQ(array.begin(), array.end());
    EXPECT_EQ(array.allocator(), &allocator);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);

    EXPECT_TRUE(array._arr_clear());
    EXPECT_EQ(array.allocator(), &allocator);
    EXPECT_TRUE(array._arr_shutdown());
    EXPECT_EQ(array.allocator(), nullptr);
}

TEST(Arr, OwnsAllocatorThroughTypedMoveOnlyToken) {
    OwnedAllocatorProbe::destruction_count = 0;
    auto owner = nk::mem::AllocatorOwner::make_native_untracked<
        OwnedAllocatorProbe>(nk::mem::untracked);
    ASSERT_TRUE(owner);
    nk::mem::Allocator* allocator = owner.get();

    nk::cl::arr<nk::u32> array;
    ASSERT_TRUE(array.arr_init_list_own(
        std::move(owner),
        {3u, 5u, 8u}));
    EXPECT_FALSE(owner);
    EXPECT_TRUE(array.owns_allocator());
    EXPECT_EQ(array.allocator(), allocator);
    EXPECT_EQ(array.length(), 3);
    EXPECT_EQ(array[0], 3u);
    EXPECT_EQ(array[1], 5u);
    EXPECT_EQ(array[2], 8u);
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 0);

    EXPECT_TRUE(array.arr_clear());
    EXPECT_TRUE(array.owns_allocator());
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 0);

    ASSERT_TRUE(array._arr_init(array.allocator(), 2));
    EXPECT_TRUE(array.arr_shutdown());
    EXPECT_FALSE(array.owns_allocator());
    EXPECT_EQ(array.allocator(), nullptr);
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 1);
}

TEST(Arr, TransfersOwnedAllocatorDuringMoveAssignment) {
    OwnedAllocatorProbe::destruction_count = 0;
    auto owner = nk::mem::AllocatorOwner::make_native_untracked<
        OwnedAllocatorProbe>(nk::mem::untracked);
    ASSERT_TRUE(owner);

    nk::mem::MallocAllocator borrowed_allocator{nk::mem::untracked};
    nk::cl::arr<nk::u32> source;
    nk::cl::arr<nk::u32> destination;
    ASSERT_TRUE(source._arr_init_own(std::move(owner), 2));
    ASSERT_TRUE(destination._arr_init(&borrowed_allocator, 4));

    destination = std::move(source);
    EXPECT_TRUE(destination.owns_allocator());
    EXPECT_FALSE(source.owns_allocator());
    EXPECT_EQ(source.allocator(), nullptr);
    EXPECT_EQ(borrowed_allocator.get_active_allocation_count(), 0);
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 0);

    EXPECT_TRUE(destination._arr_shutdown());
    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 1);
}

TEST(Arr, DestructorReleasesOwnedAllocatorAfterItsStorage) {
    OwnedAllocatorProbe::destruction_count = 0;
    Probe::reset();

    {
        auto owner = nk::mem::AllocatorOwner::make_native_untracked<
            OwnedAllocatorProbe>(nk::mem::untracked);
        ASSERT_TRUE(owner);

        nk::cl::arr<Probe> array;
        ASSERT_TRUE(array._arr_init_own(std::move(owner), 2));
        EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 0);
    }

    EXPECT_EQ(OwnedAllocatorProbe::destruction_count, 1);
    EXPECT_EQ(Probe::live_count, 0);
}

TEST(Arr, RejectsErasedAllocatorOwnershipWithoutChangingState) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::mem::Allocator* erased_allocator = &allocator;
    nk::cl::arr<nk::u32> array;

    EXPECT_FALSE(array._arr_init_own(erased_allocator, 3));
    EXPECT_EQ(array.data(), nullptr);
    EXPECT_EQ(array.length(), 0);
    EXPECT_EQ(array.allocator(), nullptr);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Arr, CopiesAndMovesFromDynamicArraysWithoutManualReset) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};

    nk::cl::dyarr<nk::u32> copy_source;
    copy_source._dyarr_init_list(&allocator, {2, 4, 8});
    nk::cl::arr<nk::u32> copy{copy_source};
    ASSERT_EQ(copy.length(), 3);
    EXPECT_NE(copy.data(), copy_source.data());
    EXPECT_EQ(copy[0], 2u);
    EXPECT_EQ(copy[1], 4u);
    EXPECT_EQ(copy[2], 8u);
    EXPECT_EQ(copy_source.length(), 3);

    EXPECT_TRUE(copy._arr_shutdown());
    copy_source._dyarr_shutdown();
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);

    nk::cl::dyarr<nk::u32> move_source;
    move_source._dyarr_init_list(&allocator, {3, 6, 9});
    ASSERT_GT(move_source.capacity(), move_source.length());

    nk::cl::arr<nk::u32> moved{std::move(move_source)};
    ASSERT_EQ(moved.length(), 3);
    EXPECT_EQ(moved[0], 3u);
    EXPECT_EQ(moved[1], 6u);
    EXPECT_EQ(moved[2], 9u);
    EXPECT_EQ(move_source.data(), nullptr);
    EXPECT_EQ(move_source.length(), 0);
    EXPECT_EQ(move_source.capacity(), 0);
    EXPECT_EQ(move_source.allocator(), nullptr);
    EXPECT_EQ(allocator.get_active_allocation_count(), 1);

    EXPECT_TRUE(moved._arr_shutdown());
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}
