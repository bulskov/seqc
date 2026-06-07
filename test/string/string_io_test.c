#include <gtest/gtest.h>

#include <cstdio>
#include <unistd.h> /* dup, dup2 — redirect stdout for print/println tests */

extern "C"
{
#include "seqc/string.h"
#include "seqc/string_io.h"
}

TEST(string_io, fwrite_writes_exact_bytes)
{
    FILE *f = tmpfile();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(string_fwrite(STRING_LIT("hello"), f), 5u);
    rewind(f);
    char buf[16] = {0};
    EXPECT_EQ(fread(buf, 1, sizeof buf - 1, f), 5u);
    EXPECT_STREQ(buf, "hello");
    fclose(f);
}

TEST(string_io, fwrite_is_binary_safe_with_embedded_nul)
{
    FILE *f = tmpfile();
    ASSERT_NE(f, nullptr);
    const char data[] = {'a', '\0', 'b'};
    string_t s = {data, 3};
    EXPECT_EQ(string_fwrite(s, f), 3u);
    rewind(f);
    char buf[8] = {0};
    ASSERT_EQ(fread(buf, 1, sizeof buf, f), 3u);
    EXPECT_EQ(buf[0], 'a');
    EXPECT_EQ(buf[1], '\0');
    EXPECT_EQ(buf[2], 'b');
    fclose(f);
}

TEST(string_io, fwrite_null_stream_returns_zero)
{
    EXPECT_EQ(string_fwrite(STRING_LIT("x"), nullptr), 0u);
}

TEST(string_io, fwrite_empty_string_returns_zero)
{
    FILE *f = tmpfile();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(string_fwrite((string_t){NULL, 0}, f), 0u);
    fclose(f);
}

/* print/println go to stdout; redirect it to a temp file, capture, restore. */
TEST(string_io, print_and_println_to_stdout)
{
    FILE *f = tmpfile();
    ASSERT_NE(f, nullptr);
    int saved = dup(fileno(stdout));
    ASSERT_NE(saved, -1);
    fflush(stdout);
    ASSERT_NE(dup2(fileno(f), fileno(stdout)), -1);

    size_t a = string_print(STRING_LIT("ab"));
    size_t b = string_println(STRING_LIT("cd"));
    fflush(stdout);

    /* restore the real stdout before asserting, so test output is visible */
    dup2(saved, fileno(stdout));
    close(saved);

    EXPECT_EQ(a, 2u);
    EXPECT_EQ(b, 3u); /* "cd" + '\n' */

    rewind(f);
    char buf[16] = {0};
    EXPECT_EQ(fread(buf, 1, sizeof buf - 1, f), 5u);
    EXPECT_STREQ(buf, "abcd\n");
    fclose(f);
}
