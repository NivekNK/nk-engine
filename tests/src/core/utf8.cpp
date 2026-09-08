#include <gtest/gtest.h>
#include "core/utf8.h"

using namespace nk;

TEST(Utf8, RoundTripsUnicodeScalarsWithoutChangingByteLengths) {
    for (u32 value : {0u, 0x7fu, 0x80u, 0x7ffu, 0x800u, 0xd7ffu,
                      0xe000u, 0xffffu, 0x10000u, 0x1f642u, 0x10ffffu}) {
        char bytes[4]{};
        const u8 count = utf8::encode(value, bytes);
        ASSERT_NE(count, 0);
        const strview view{bytes, count};
        auto decoded = utf8::decode(view, 0);
        ASSERT_TRUE(decoded);
        EXPECT_EQ(decoded->value, value);
        EXPECT_EQ(decoded->bytes, count);
        EXPECT_EQ(view.length(), count);
        EXPECT_TRUE(utf8::valid(view));
        EXPECT_FALSE(utf8::decode(view, count));
        if (count > 1) { EXPECT_FALSE(utf8::valid({bytes, count - 1u})); }
    }
}

TEST(Utf8, RejectsOverlongSurrogateOutOfRangeAndBrokenSequences) {
    for (strview value : {strview{"\x80"}, strview{"\xc0\xaf"},
        strview{"\xe0\x80\x80"}, strview{"\xed\xa0\x80"},
        strview{"\xf0\x80\x80\x80"}, strview{"\xf4\x90\x80\x80"},
        strview{"\xff"}, strview{"\xe2X\xa1"}, strview{"\xf0\x9f"}})
        EXPECT_FALSE(utf8::valid(value));
    char bytes[4]{};
    EXPECT_EQ(utf8::encode(0xd800, bytes), 0);
    EXPECT_EQ(utf8::encode(0x110000, bytes), 0);
    EXPECT_TRUE(utf8::valid({}));
    EXPECT_TRUE(utf8::valid("¡Hola! áéíóú ñ — 🙂"));
}
