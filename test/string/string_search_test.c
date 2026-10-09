/* Searching, splitting and case-insensitive comparison of string_t:
 *
 *   size_t string_find_char(string_t s, char c);
 *   size_t string_rfind_char(string_t s, char c);
 *   bool   string_split_next(string_t *rest, char sep, string_t *token);
 *   int    string_compare_case_insensitive(string_t a, string_t b);
 *   size_t string_hash_case_insensitive(const void *key, size_t key_size);
 *   bool   string_key_eq_case_insensitive(const void *a, const void *b,
 *                                         size_t key_size);
 *
 * None of them allocates.  All of them work on exactly s.len bytes: a
 * string_t is not a C string, so embedded NULs are ordinary bytes and
 * nothing past len is read.  "Case-insensitive" means ASCII letters only. */

#include "ctt.h"

#include <stdio.h>
#include <string.h>

#include "arena/growing_arena.h"
#include "seqc/hashmap.h"
#include "seqc/string.h"

#define L(lit) STRING_LIT(lit)

/* A string_t over the first n bytes of a buffer that continues with more
 * bytes — anything that reads past len sees them. */
static string_t prefix(const char *buf, size_t n)
{
    return (string_t){buf, n};
}

/* --- string_find_char --------------------------------------------------- */

TEST(find_char_returns_first_position)
{
    ASSERT_EQ(2, string_find_char(L("hello"), 'l'));
    ASSERT_EQ(0, string_find_char(L("hello"), 'h'));
    ASSERT_EQ(4, string_find_char(L("hello"), 'o'));
}

TEST(find_char_not_found)
{
    ASSERT_EQ(STRING_NOT_FOUND, string_find_char(L("hello"), 'z'));
    ASSERT_EQ(STRING_NOT_FOUND, string_find_char(L(""), 'a'));
    ASSERT_EQ(STRING_NOT_FOUND, string_find_char((string_t){NULL, 0}, 'a'));
}

TEST(find_char_treats_nul_as_an_ordinary_byte)
{
    const char data[] = {'a', '\0', 'b'};
    string_t s = {data, 3};
    ASSERT_EQ(1, string_find_char(s, '\0'));
    ASSERT_EQ(2, string_find_char(s, 'b')); /* the NUL does not stop it */
}

TEST(find_char_reads_nothing_past_len)
{
    ASSERT_EQ(STRING_NOT_FOUND, string_find_char(prefix("abcX", 3), 'X'));
}

/* Bytes above 127 (UTF-8) must not be confused with negative chars. */
TEST(find_char_finds_high_bytes)
{
    string_t s = L("a\xc3\xa5"); /* "aå" */
    ASSERT_EQ(1, string_find_char(s, '\xc3'));
    ASSERT_EQ(2, string_find_char(s, '\xa5'));
}

/* --- string_rfind_char -------------------------------------------------- */

TEST(rfind_char_returns_last_position)
{
    ASSERT_EQ(3, string_rfind_char(L("hello"), 'l'));
    ASSERT_EQ(0, string_rfind_char(L("hello"), 'h'));
    ASSERT_EQ(4, string_rfind_char(L("hello"), 'o'));
    ASSERT_EQ(4, string_rfind_char(L("/a/b/c"), '/'));
    ASSERT_EQ(0, string_rfind_char(L("/abc"), '/'));
}

TEST(rfind_char_not_found)
{
    ASSERT_EQ(STRING_NOT_FOUND, string_rfind_char(L("hello"), 'z'));
    ASSERT_EQ(STRING_NOT_FOUND, string_rfind_char(L(""), 'a'));
    ASSERT_EQ(STRING_NOT_FOUND, string_rfind_char((string_t){NULL, 0}, 'a'));
}

TEST(rfind_char_reads_nothing_past_len)
{
    ASSERT_EQ(STRING_NOT_FOUND, string_rfind_char(prefix("abc/", 3), '/'));
    ASSERT_EQ(1, string_rfind_char(prefix("a/c/", 3), '/'));
}

/* --- string_split_next -------------------------------------------------- */

/* Run the tokenizer over s and join the tokens with '|', wrapping each in
 * [] so empty tokens show: "a/b" -> "[a]|[b]".  Also counts them. */
static const char *tokens(string_t s, char sep, int *count)
{
    static char out[256];
    size_t len = 0;
    int n = 0;
    out[0] = '\0';
    string_t rest = s, tok;
    while (string_split_next(&rest, sep, &tok))
    {
        len += (size_t)snprintf(
            out + len,
            sizeof out - len,
            "%s[%.*s]",
            n ? "|" : "",
            (int)tok.len,
            tok.ptr ? tok.ptr : "");
        n++;
    }
    if (count)
    {
        *count = n;
    }
    return out;
}

TEST(split_next_splits_at_each_separator)
{
    ASSERT_STR_EQ("[a]|[b]|[c]", tokens(L("a/b/c"), '/', NULL));
    ASSERT_STR_EQ("[usr]|[lib]", tokens(L("usr/lib"), '/', NULL));
}

TEST(split_next_without_separator_gives_the_whole_string)
{
    ASSERT_STR_EQ("[abc]", tokens(L("abc"), '/', NULL));
}

/* Empty pieces are kept: before a leading separator, between two, and
 * after a trailing one. */
TEST(split_next_keeps_empty_pieces)
{
    ASSERT_STR_EQ("[a]|[]|[b]", tokens(L("a//b"), '/', NULL));
    ASSERT_STR_EQ("[]|[a]", tokens(L("/a"), '/', NULL));
    ASSERT_STR_EQ("[a]|[]", tokens(L("a/"), '/', NULL));
    ASSERT_STR_EQ("[]|[]", tokens(L("/"), '/', NULL));
    ASSERT_STR_EQ("[a]|[]|[b]|[]", tokens(L("a//b/"), '/', NULL));
}

TEST(split_next_on_empty_and_null)
{
    int n = -1;
    ASSERT_STR_EQ("[]", tokens(L(""), '/', &n));
    ASSERT_EQ(1, n); /* "" is one empty piece */
    tokens((string_t){NULL, 0}, '/', &n);
    ASSERT_EQ(0, n); /* {NULL, 0} is nothing at all */
}

/* After the last piece, rest is {NULL, 0} and further calls keep
 * returning false without touching token. */
TEST(split_next_stays_finished)
{
    string_t rest = L("a"), tok;
    ASSERT_TRUE(string_split_next(&rest, '/', &tok));
    ASSERT_NULL(rest.ptr);
    ASSERT_EQ(0, rest.len);
    tok = L("untouched");
    ASSERT_FALSE(string_split_next(&rest, '/', &tok));
    ASSERT_FALSE(string_split_next(&rest, '/', &tok));
    ASSERT_STR_EQ("untouched", tok.ptr);
}

/* Tokens are views into the input; rest advances through it. */
TEST(split_next_returns_views)
{
    string_t s = L("ab/cd");
    string_t rest = s, tok;
    ASSERT_TRUE(string_split_next(&rest, '/', &tok));
    ASSERT_PTR_EQ(s.ptr, tok.ptr);
    ASSERT_EQ(2, tok.len);
    ASSERT_PTR_EQ(s.ptr + 3, rest.ptr); /* just past the '/' */
    ASSERT_EQ(2, rest.len);
    ASSERT_TRUE(string_split_next(&rest, '/', &tok));
    ASSERT_PTR_EQ(s.ptr + 3, tok.ptr);
    ASSERT_EQ(2, tok.len);
}

TEST(split_next_reads_nothing_past_len)
{
    ASSERT_STR_EQ("[a]|[b]", tokens(prefix("a/b/zzz", 3), '/', NULL));
}

/* Any byte can be the separator, including '\\' and NUL. */
TEST(split_next_with_other_separators)
{
    ASSERT_STR_EQ("[C:]|[Users]|[me]", tokens(L("C:\\Users\\me"), '\\', NULL));
    const char data[] = {'a', '\0', 'b'};
    ASSERT_STR_EQ("[a]|[b]", tokens((string_t){data, 3}, '\0', NULL));
}

/* --- string_compare_case_insensitive ------------------------------------ */

TEST(compare_ci_ignores_ascii_case)
{
    ASSERT_EQ(0, string_compare_case_insensitive(L("abc"), L("ABC")));
    ASSERT_EQ(
        0, string_compare_case_insensitive(L("ReadMe.MD"), L("readme.md")));
    ASSERT_EQ(0, string_compare_case_insensitive(L(""), L("")));
}

TEST(compare_ci_orders_like_lower_case)
{
    ASSERT_LT(string_compare_case_insensitive(L("abc"), L("abd")), 0);
    ASSERT_LT(string_compare_case_insensitive(L("ABC"), L("abd")), 0);
    ASSERT_GT(string_compare_case_insensitive(L("abd"), L("ABC")), 0);
    ASSERT_LT(string_compare_case_insensitive(L("a"), L("B")), 0);
    /* Folding is to LOWER case: 'B' becomes 'b' (0x62), above '_' (0x5f).
     * Folding to upper case would put 'B' (0x42) below '_'. */
    ASSERT_GT(string_compare_case_insensitive(L("B"), L("_")), 0);
}

TEST(compare_ci_shorter_prefix_sorts_first)
{
    ASSERT_LT(string_compare_case_insensitive(L("ab"), L("ABC")), 0);
    ASSERT_GT(string_compare_case_insensitive(L("ABC"), L("ab")), 0);
    ASSERT_LT(string_compare_case_insensitive(L(""), L("a")), 0);
}

/* Only ASCII letters fold: 'å' (c3 a5) and 'Å' (c3 85) stay different, and
 * the result does not depend on the C locale. */
TEST(compare_ci_folds_ascii_only)
{
    ASSERT_NE(0, string_compare_case_insensitive(L("\xc3\xa5"), L("\xc3\x85")));
    ASSERT_NE(0, string_compare_case_insensitive(L("1"), L("!")));
}

TEST(compare_ci_agrees_with_equals_ci)
{
    static const char *pairs[][2] = {
        {"abc", "ABC"}, {"abc", "abd"}, {"a", "a "}, {"Zz", "zZ"}, {"", "x"}};
    for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; ++i)
    {
        string_t a = string_view_cstr(pairs[i][0]);
        string_t b = string_view_cstr(pairs[i][1]);
        ASSERT_EQ(
            string_equals_case_insensitive(a, b),
            string_compare_case_insensitive(a, b) == 0);
    }
}

TEST(compare_ci_reads_nothing_past_len)
{
    ASSERT_EQ(
        0,
        string_compare_case_insensitive(prefix("abcX", 3), prefix("ABCY", 3)));
}

/* --- string_hash_case_insensitive / string_key_eq_case_insensitive ------- */

static size_t hash_ci(string_t s)
{
    return string_hash_case_insensitive(&s, sizeof s);
}

static bool eq_ci(string_t a, string_t b)
{
    return string_key_eq_case_insensitive(&a, &b, sizeof a);
}

TEST(hash_ci_is_equal_for_case_variants)
{
    ASSERT_EQ(hash_ci(L("README.md")), hash_ci(L("readme.MD")));
    ASSERT_EQ(hash_ci(L("C:")), hash_ci(L("c:")));
    ASSERT_EQ(hash_ci(L("")), hash_ci(L("")));
}

/* Not required for correctness, but a hash that ignores content is
 * useless. */
TEST(hash_ci_differs_for_different_strings)
{
    ASSERT_NE(hash_ci(L("abc")), hash_ci(L("abd")));
    ASSERT_NE(hash_ci(L("ab")), hash_ci(L("abc")));
}

TEST(hash_ci_reads_nothing_past_len)
{
    ASSERT_EQ(hash_ci(prefix("abcX", 3)), hash_ci(prefix("ABCY", 3)));
}

TEST(key_eq_ci)
{
    ASSERT_TRUE(eq_ci(L("README.md"), L("readme.MD")));
    ASSERT_FALSE(eq_ci(L("readme"), L("readme.md")));
    ASSERT_FALSE(eq_ci(L("\xc3\xa5"), L("\xc3\x85"))); /* å vs Å */
}

/* The point of the pair: a hashmap whose string keys ignore case. */
TEST(hashmap_with_case_insensitive_keys)
{
    growing_arena_t arena;
    growing_arena_init(&arena, 4096);
    hashmap_t *map = hashmap_create(
        sizeof(string_t),
        sizeof(int),
        string_hash_case_insensitive,
        string_key_eq_case_insensitive,
        growing_arena_allocator(&arena));
    ASSERT_NOT_NULL(map);

    string_t key = L("Program Files");
    int value = 42;
    ASSERT_EQ(SEQC_OK, hashmap_set(map, &key, &value));

    string_t other_case = L("PROGRAM FILES");
    int out = 0;
    ASSERT_EQ(SEQC_OK, hashmap_get(map, &other_case, &out));
    ASSERT_EQ(42, out);

    /* Setting the other spelling replaces, it does not add. */
    int value2 = 7;
    ASSERT_EQ(SEQC_OK, hashmap_set(map, &other_case, &value2));
    ASSERT_EQ(1, hashmap_len(map));

    hashmap_destroy(map);
    growing_arena_destroy(&arena);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "string search");
}
