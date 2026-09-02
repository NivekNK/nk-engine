#include <concepts>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "core/result.h"
#include "memory/malloc_allocator.h"

namespace {
    enum class SmallError : nk::u8 {
        invalid,
        unavailable,
    };

    struct MoveOnly {
        explicit MoveOnly(const int initial_value) noexcept
            : value(initial_value) {}

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;

        MoveOnly(MoveOnly&& other) noexcept
            : value(other.value) {
            other.value = -1;
        }

        MoveOnly& operator=(MoveOnly&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }

        int value;
    };

    struct MoveOnlyError {
        explicit MoveOnlyError(const int initial_code) noexcept
            : code(initial_code) {}

        MoveOnlyError(const MoveOnlyError&) = delete;
        MoveOnlyError& operator=(const MoveOnlyError&) = delete;

        MoveOnlyError(MoveOnlyError&& other) noexcept
            : code(other.code) {
            other.code = -1;
        }

        MoveOnlyError& operator=(MoveOnlyError&& other) noexcept {
            code = other.code;
            other.code = -1;
            return *this;
        }

        int code;
    };

    struct LifetimeProbe {
        explicit LifetimeProbe(const int initial_value) noexcept
            : value(initial_value) {
            ++direct_constructions;
            ++alive;
        }

        LifetimeProbe(const LifetimeProbe& other) noexcept
            : value(other.value) {
            ++copy_constructions;
            ++alive;
        }

        LifetimeProbe(LifetimeProbe&& other) noexcept
            : value(other.value) {
            other.value = -1;
            ++move_constructions;
            ++alive;
        }

        LifetimeProbe& operator=(const LifetimeProbe&) = delete;
        LifetimeProbe& operator=(LifetimeProbe&&) = delete;

        ~LifetimeProbe() {
            ++destructions;
            --alive;
        }

        static void reset() noexcept {
            direct_constructions = 0;
            copy_constructions = 0;
            move_constructions = 0;
            destructions = 0;
            alive = 0;
        }

        static int total_constructions() noexcept {
            return direct_constructions + copy_constructions + move_constructions;
        }

        int value;

        static inline int direct_constructions = 0;
        static inline int copy_constructions = 0;
        static inline int move_constructions = 0;
        static inline int destructions = 0;
        static inline int alive = 0;
    };

    struct Pair {
        int left;
        int right;
    };

    using TrivialResult = nk::result<nk::u64, SmallError>;
    using MoveOnlyResult = nk::result<MoveOnly, SmallError>;
    using MoveOnlyErrorResult = nk::result<int, MoveOnlyError>;

    static_assert(!std::default_initializable<TrivialResult>);
    static_assert(std::is_trivially_destructible_v<TrivialResult>);
    static_assert(std::is_trivially_copyable_v<TrivialResult>);
    static_assert(std::is_trivially_copyable_v<nk::result<void, SmallError>>);
    static_assert(sizeof(TrivialResult) <= 16);
    static_assert(sizeof(nk::result<void, SmallError>) <= 2);
    static_assert(!std::copy_constructible<MoveOnlyResult>);
    static_assert(std::move_constructible<MoveOnlyResult>);
    static_assert(!std::copy_constructible<MoveOnlyErrorResult>);
    static_assert(std::move_constructible<MoveOnlyErrorResult>);

    constexpr nk::result<int, SmallError> constexpr_success = nk::ok(42);
    constexpr nk::result<int, SmallError> constexpr_failure =
        nk::err(SmallError::unavailable);
    static_assert(constexpr_success.has_value());
    static_assert(constexpr_success.value() == 42);
    static_assert(!constexpr_failure.has_value());
    static_assert(constexpr_failure.error() == SmallError::unavailable);

    void access_value_on_failure() {
        nk::result<int, SmallError> failure = nk::err(SmallError::invalid);
        (void)failure.value();
    }

    void access_error_on_success() {
        nk::result<int, SmallError> success = nk::ok(42);
        (void)success.error();
    }

    void access_void_value_on_failure() {
        nk::result<void, SmallError> failure = nk::err(SmallError::invalid);
        failure.value();
    }
}

TEST(Result, DistinguishesSuccessAndFailureWhenTypesAreEqual) {
    nk::result<int, int> success = nk::ok(42);
    nk::result<int, int> failure = nk::err(7);

    ASSERT_TRUE(success);
    EXPECT_EQ(success.value(), 42);
    EXPECT_EQ(*success, 42);

    ASSERT_FALSE(failure);
    EXPECT_EQ(failure.error(), 7);
}

TEST(Result, SupportsPointerOperatorsAndValueFallbacks) {
    const nk::result<Pair, SmallError> success = nk::ok(Pair{3, 5});
    const nk::result<Pair, SmallError> failure = nk::err(SmallError::invalid);

    EXPECT_EQ(success->left, 3);
    EXPECT_EQ((*success).right, 5);
    EXPECT_EQ(success.value_or(Pair{11, 13}).left, 3);
    EXPECT_EQ(failure.value_or(Pair{11, 13}).right, 13);
}

TEST(Result, MovesValuesAndErrorsWithoutRequiringCopies) {
    MoveOnlyResult value = nk::ok(MoveOnly{19});
    MoveOnlyResult moved_value = std::move(value);
    ASSERT_TRUE(moved_value);
    EXPECT_EQ(moved_value->value, 19);

    MoveOnlyErrorResult failure = nk::err(MoveOnlyError{23});
    MoveOnlyErrorResult moved_failure = std::move(failure);
    ASSERT_FALSE(moved_failure);
    EXPECT_EQ(moved_failure.error().code, 23);

    MoveOnly fallback{31};
    MoveOnly extracted = std::move(moved_value).value_or(std::move(fallback));
    EXPECT_EQ(extracted.value, 19);
}

TEST(Result, ReconstructsAlternativesAndHandlesSelfAssignment) {
    LifetimeProbe::reset();
    {
        using ProbeResult = nk::result<LifetimeProbe, LifetimeProbe>;
        ProbeResult current = nk::ok(LifetimeProbe{1});
        ProbeResult failure = nk::err(LifetimeProbe{2});
        ASSERT_TRUE(current);
        ASSERT_FALSE(failure);

        current = failure;
        ASSERT_FALSE(current);
        EXPECT_EQ(current.error().value, 2);

        current = current;
        EXPECT_EQ(current.error().value, 2);
        ProbeResult* self = &current;
        current = std::move(*self);
        EXPECT_EQ(current.error().value, 2);

        ProbeResult success = nk::ok(LifetimeProbe{7});
        current = std::move(success);
        ASSERT_TRUE(current);
        EXPECT_EQ(current.value().value, 7);
    }

    EXPECT_EQ(LifetimeProbe::alive, 0);
    EXPECT_EQ(
        LifetimeProbe::destructions,
        LifetimeProbe::total_constructions());
}

TEST(Result, RepresentsVoidSuccessWithoutValueStorage) {
    nk::result<void, SmallError> success = nk::ok();
    nk::result<void, SmallError> failure = nk::err(SmallError::unavailable);

    ASSERT_TRUE(success);
    success.value();
    ASSERT_FALSE(failure);
    EXPECT_EQ(failure.error(), SmallError::unavailable);

    success = failure;
    ASSERT_FALSE(success);
    EXPECT_EQ(success.error(), SmallError::unavailable);
    success = nk::result<void, SmallError>{nk::ok()};
    EXPECT_TRUE(success);
}

TEST(Result, DoesNotUseEngineAllocatorsForWrapperOperations) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    for (nk::u64 index = 0; index < 1000; ++index) {
        TrivialResult value = nk::ok(index);
        TrivialResult copy = value;
        value = copy;
        EXPECT_EQ(value.value(), index);
    }

    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
    EXPECT_EQ(allocator.get_used_bytes(), 0);
}

TEST(ResultDeathTest, RejectsValueAccessOnFailure) {
    EXPECT_DEATH(access_value_on_failure(), "");
}

TEST(ResultDeathTest, RejectsErrorAccessOnSuccess) {
    EXPECT_DEATH(access_error_on_success(), "");
}

TEST(ResultDeathTest, RejectsVoidValueAccessOnFailure) {
    EXPECT_DEATH(access_void_value_on_failure(), "");
}
