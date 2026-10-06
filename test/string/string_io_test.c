#include "ctt.h"

#include <stdio.h>
#include <unistd.h> /* dup, dup2 — redirect stdout for print/println tests */

#include "seqc/string.h"
#include "seqc/string_io.h"

TEST(string_io_fwrite_writes_exact_bytes)
{
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    ASSERT_EQ(5u, string_fwrite(STRING_LIT("hello"), f));
    rewind(f);
    char buf[16] = {0};
    ASSERT_EQ(5u, fread(buf, 1, sizeof buf - 1, f));
    ASSERT_STR_EQ("hello", buf);
    fclose(f);
}

TEST(string_io_fwrite_is_binary_safe_with_embedded_nul)
{
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    const char data[] = {'a', '\0', 'b'};
    string_t s = {data, 3};
    ASSERT_EQ(3u, string_fwrite(s, f));
    rewind(f);
    char buf[8] = {0};
    ASSERT_EQ(3u, fread(buf, 1, sizeof buf, f));
    ASSERT_EQ('a', buf[0]);
    ASSERT_EQ('\0', buf[1]);
    ASSERT_EQ('b', buf[2]);
    fclose(f);
}

TEST(string_io_fwrite_null_stream_returns_zero)
{
    ASSERT_EQ(0u, string_fwrite(STRING_LIT("x"), NULL));
}

TEST(string_io_fwrite_empty_string_returns_zero)
{
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    ASSERT_EQ(0u, string_fwrite((string_t){NULL, 0}, f));
    fclose(f);
}

/* print/println go to stdout; redirect it to a temp file, capture, restore. */
TEST(string_io_print_and_println_to_stdout)
{
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    int saved = dup(fileno(stdout));
    ASSERT_NE(-1, saved);
    fflush(stdout);
    ASSERT_NE(-1, dup2(fileno(f), fileno(stdout)));

    size_t a = string_print(STRING_LIT("ab"));
    size_t b = string_println(STRING_LIT("cd"));
    fflush(stdout);

    /* restore the real stdout before asserting, so test output is visible */
    dup2(saved, fileno(stdout));
    close(saved);

    ASSERT_EQ(2u, a);
    ASSERT_EQ(3u, b); /* "cd" + '\n' */

    rewind(f);
    char buf[16] = {0};
    ASSERT_EQ(5u, fread(buf, 1, sizeof buf - 1, f));
    ASSERT_STR_EQ("abcd\n", buf);
    fclose(f);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "string_io_test");
}
