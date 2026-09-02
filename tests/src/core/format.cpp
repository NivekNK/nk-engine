#include <gtest/gtest.h>

#include "core/format.h"
#include "memory/malloc_allocator.h"

TEST(Format, FormatsRepositorySubsetWithoutAllocating) {
    nk::strbuf<128> output;
    const nk::FormatResult result = nk::format_to(
        output,
        "{} {:>6} {:05} {:.2f} {} {}",
        "text",
        'x',
        -12,
        3.14159,
        true,
        42u);

    EXPECT_TRUE(result.success());
    EXPECT_FALSE(output.truncated());
    EXPECT_EQ(output.view(), "text      x -0012 3.14 true 42");
}

TEST(Format, SupportsEscapesBasesAlignmentAndOwnedText) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::str owned{allocator, "engine"};
    nk::strbuf<128> output;

    const nk::FormatResult result = nk::format_to(
        output,
        "{{{:*^10}}} {:X} {:b}",
        owned,
        48879u,
        10u);

    EXPECT_TRUE(result.success());
    EXPECT_EQ(output.view(), "{**engine**} BEEF 1010");
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(Format, FormatsPointersAndReportsTruncation) {
    nk::strbuf<8> output;
    int value = 1;

    const nk::FormatResult result = nk::format_to(
        output,
        "pointer={:p}",
        &value);

    EXPECT_TRUE(result.truncated());
    EXPECT_TRUE(output.truncated());
    EXPECT_EQ(output.length(), output.capacity());
    EXPECT_EQ(output.cstr()[output.length()], '\0');
}
