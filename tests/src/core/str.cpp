#include <concepts>
#include <type_traits>

#include <gtest/gtest.h>

#include "core/str.h"
#include "memory/malloc_allocator.h"

namespace {
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

    private:
        bool m_reject_allocations = false;
    };

    static_assert(!std::default_initializable<nk::str>);
    static_assert(std::constructible_from<
                  nk::str,
                  nk::mem::Allocator&,
                  nk::cstr>);
    static_assert(!std::constructible_from<nk::str, nk::cstr>);
}

TEST(Str, UsesInlineStorageAndPreservesNullTermination) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::str text{allocator, "short"};

    EXPECT_EQ(text.allocator(), &allocator);
    EXPECT_EQ(text.length(), 5);
    EXPECT_EQ(text.capacity(), nk::str::inline_capacity);
    EXPECT_EQ(text.cstr()[text.length()], '\0');
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);

    EXPECT_TRUE(text.append(" text"));
    EXPECT_EQ(text, "short text");
    EXPECT_EQ(text.cstr()[text.length()], '\0');
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Str, GrowsTransactionallyAndSupportsAliasedOperations) {
    ToggleAllocator allocator;
    nk::str text{allocator, "01234567890123456789012"};
    ASSERT_EQ(text.length(), nk::str::inline_capacity);

    allocator.reject_allocations(true);
    EXPECT_FALSE(text.append("x"));
    EXPECT_EQ(text, "01234567890123456789012");
    EXPECT_EQ(text.capacity(), nk::str::inline_capacity);

    allocator.reject_allocations(false);
    ASSERT_TRUE(text.append(text.view().substr(1, 4)));
    EXPECT_EQ(text, "012345678901234567890121234");
    EXPECT_GT(text.capacity(), nk::str::inline_capacity);
    EXPECT_EQ(allocator.get_active_allocation_count(), 1);

    ASSERT_TRUE(text.assign(text.view().substr(2, 7)));
    EXPECT_EQ(text, "2345678");
    EXPECT_EQ(text.cstr()[text.length()], '\0');
}

TEST(Str, AppliesAllocatorRulesToCopyAndMove) {
    nk::mem::MallocAllocator first_allocator{nk::mem::untracked};
    nk::mem::MallocAllocator second_allocator{nk::mem::untracked};
    constexpr nk::cstr long_text = "text that exceeds the small inline string capacity";

    nk::str source{first_allocator, long_text};
    nk::str inherited_copy{source};
    nk::str explicit_copy{second_allocator, source};

    EXPECT_EQ(inherited_copy.allocator(), &first_allocator);
    EXPECT_EQ(explicit_copy.allocator(), &second_allocator);
    EXPECT_EQ(inherited_copy, source.view());
    EXPECT_EQ(explicit_copy, source.view());

    nk::str destination{second_allocator, "destination"};
    destination = source;
    EXPECT_EQ(destination.allocator(), &second_allocator);
    EXPECT_EQ(destination, source.view());

    nk::str moved{std::move(inherited_copy)};
    EXPECT_EQ(moved.allocator(), &first_allocator);
    EXPECT_EQ(moved, long_text);
    EXPECT_EQ(inherited_copy.allocator(), nullptr);

    nk::str cross_domain_source{first_allocator, "cross-domain move contents"};
    destination = std::move(cross_domain_source);
    EXPECT_EQ(destination.allocator(), &second_allocator);
    EXPECT_EQ(destination, "cross-domain move contents");
    EXPECT_TRUE(cross_domain_source.empty());
    EXPECT_EQ(cross_domain_source.allocator(), &first_allocator);

    nk::str cloned = source.clone(second_allocator);
    EXPECT_EQ(cloned.allocator(), &second_allocator);
    EXPECT_EQ(cloned, source.view());
}
