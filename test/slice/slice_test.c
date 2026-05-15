#include <gtest/gtest.h>

extern "C"
{
#include "seqc/slice.h"
}

TEST(slice, get_first_element)
{
    int data[] = {10, 20, 30};
    Slice s = {data, 3, sizeof(int)};
    EXPECT_EQ(*(int *)slice_get(s, 0), 10);
}

TEST(slice, get_last_element)
{
    int data[] = {10, 20, 30};
    Slice s = {data, 3, sizeof(int)};
    EXPECT_EQ(*(int *)slice_get(s, 2), 30);
}

TEST(slice, get_middle_element)
{
    double data[] = {1.1, 2.2, 3.3};
    Slice s = {data, 3, sizeof(double)};
    EXPECT_NEAR(*(double *)slice_get(s, 1), 2.2, 1e-9);
}

/* ---- slice_find / slice_contains ---------------------------------------- */

static bool int_gt_three(const void *elem, void *ctx)
{
    (void)ctx;
    return *(const int *)elem > 3;
}

TEST(slice, find_returns_first_match)
{
    int data[] = {1, 2, 4, 3, 5};
    Slice s = {data, 5, sizeof(int)};
    int *p = (int *)slice_find(s, int_gt_three, NULL);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(*p, 4);
}

TEST(slice, find_returns_null_when_no_match)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    EXPECT_EQ(slice_find(s, int_gt_three, NULL), nullptr);
}

TEST(slice, contains_true_when_match_exists)
{
    int data[] = {1, 5, 2};
    Slice s = {data, 3, sizeof(int)};
    EXPECT_TRUE(slice_contains(s, int_gt_three, NULL));
}

TEST(slice, contains_false_when_no_match)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    EXPECT_TRUE(!slice_contains(s, int_gt_three, NULL));
}

/* slice_get with an out-of-bounds index must return NULL. */
TEST(slice, get_out_of_bounds_returns_null)
{
    int data[] = {10, 20};
    Slice s = {data, 2, sizeof(int)};
    EXPECT_EQ(slice_get(s, 2), nullptr);
    EXPECT_EQ(slice_get(s, 99), nullptr);
}
