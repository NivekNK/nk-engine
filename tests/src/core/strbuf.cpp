#include <gtest/gtest.h>

#include "core/strbuf.h"

TEST(Strbuf, KeepsInlineTextNullTerminated) {
    nk::strbuf<8> text;
    EXPECT_TRUE(text.append("abc"));
    EXPECT_TRUE(text.append('-'));
    EXPECT_TRUE(text.append("defg"));

    EXPECT_EQ(text.length(), 8);
    EXPECT_EQ(text.capacity(), 8);
    EXPECT_EQ(text.view(), "abc-defg");
    EXPECT_EQ(text.cstr()[text.length()], '\0');
    EXPECT_FALSE(text.truncated());
}

TEST(Strbuf, ReportsTruncationWithoutSpilling) {
    nk::strbuf<5> text;
    EXPECT_FALSE(text.append("12345678"));
    EXPECT_EQ(text.view(), "12345");
    EXPECT_TRUE(text.truncated());
    EXPECT_EQ(text.cstr()[5], '\0');

    EXPECT_FALSE(text.append('x'));
    EXPECT_TRUE(text.truncated());

    text.clear();
    EXPECT_TRUE(text.empty());
    EXPECT_FALSE(text.truncated());
    EXPECT_STREQ(text.cstr(), "");
}

TEST(Strbuf, AssignsAliasedSubrangesSafely) {
    nk::strbuf<8> text{"abcdefgh"};

    EXPECT_TRUE(text.assign(text.view().substr(2, 4)));
    EXPECT_EQ(text.view(), "cdef");
    EXPECT_FALSE(text.truncated());
    EXPECT_EQ(text.cstr()[text.length()], '\0');
}

TEST(Strbuf, MarksTruncatedOutputWithinItsFixedCapacity) {
    nk::strbuf<16> text;
    EXPECT_FALSE(text.append("a message that is much too long"));
    text.mark_truncated();

    EXPECT_EQ(text.length(), 16u);
    EXPECT_EQ(text.view(), "a ...<truncated>");
    EXPECT_TRUE(text.truncated());
    EXPECT_EQ(text.cstr()[text.length()], '\0');
}
