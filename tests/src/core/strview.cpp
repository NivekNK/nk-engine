#include <gtest/gtest.h>

#include "core/str.h"
#include "core/strbuf.h"
#include "memory/malloc_allocator.h"

TEST(Strview, RepresentsTextWithoutClaimingNullTermination) {
    constexpr char text[] = {'a', 'l', 'p', 'h', 'a', '-', 'b', 'e', 't', 'a'};
    const nk::strview view{text, sizeof(text)};

    EXPECT_EQ(view.length(), 10);
    EXPECT_TRUE(view.starts_with("alpha"));
    EXPECT_TRUE(view.ends_with("beta"));
    EXPECT_EQ(view.find('-'), 5);
    EXPECT_EQ(view.find("beta"), 6);
    EXPECT_EQ(view.find("missing"), nk::strview::npos);
    EXPECT_EQ(view.substr(6), "beta");
    EXPECT_EQ(view.substr(0, 5), "alpha");
    EXPECT_TRUE(view.substr(100).empty());
}

TEST(Strview, ConstructsFromOwnedAndFixedText) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::str owned{allocator, "owned text"};
    nk::strbuf<16> fixed{"fixed text"};

    const nk::strview owned_view{owned};
    const nk::strview fixed_view{fixed};
    EXPECT_EQ(owned_view, "owned text");
    EXPECT_EQ(fixed_view, "fixed text");
    EXPECT_LT(nk::strview{"alpha"}.compare("beta"), 0);
    EXPECT_GT(nk::strview{"beta"}.compare("alpha"), 0);
    EXPECT_EQ(nk::strview{"same"}.compare("same"), 0);
}
