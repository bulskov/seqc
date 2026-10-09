#include "ctt.h"

#include "seqc/slice.h"

TEST(slice_get_first_element)
{
    int data[] = {10, 20, 30};
    slice_t s = {data, 3, sizeof(int)};
    ASSERT_EQ(10, *(int *)slice_get_ptr(s, 0));
}

TEST(slice_get_last_element)
{
    int data[] = {10, 20, 30};
    slice_t s = {data, 3, sizeof(int)};
    ASSERT_EQ(30, *(int *)slice_get_ptr(s, 2));
}

TEST(slice_get_middle_element)
{
    double data[] = {1.1, 2.2, 3.3};
    slice_t s = {data, 3, sizeof(double)};
    ASSERT_FLOAT_EQ(2.2, *(double *)slice_get_ptr(s, 1), 1e-9);
}

/* ---- slice_find / slice_any ---------------------------------------- */

static bool int_gt_three(const void *elem, void *ctx)
{
    (void)ctx;
    return *(const int *)elem > 3;
}

TEST(slice_find_returns_first_match)
{
    int data[] = {1, 2, 4, 3, 5};
    slice_t s = {data, 5, sizeof(int)};
    int *p = (int *)slice_find(s, int_gt_three, NULL);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(4, *p);
}

TEST(slice_find_returns_null_when_no_match)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    ASSERT_NULL(slice_find(s, int_gt_three, NULL));
}

TEST(slice_contains_true_when_match_exists)
{
    int data[] = {1, 5, 2};
    slice_t s = {data, 3, sizeof(int)};
    ASSERT_TRUE(slice_any(s, int_gt_three, NULL));
}

TEST(slice_contains_false_when_no_match)
{
    int data[] = {1, 2, 3};
    slice_t s = {data, 3, sizeof(int)};
    ASSERT_TRUE(!slice_any(s, int_gt_three, NULL));
}

/* slice_get_ptr with an out-of-bounds index must return NULL. */
TEST(slice_get_out_of_bounds_returns_null)
{
    int data[] = {10, 20};
    slice_t s = {data, 2, sizeof(int)};
    ASSERT_NULL(slice_get_ptr(s, 2));
    ASSERT_NULL(slice_get_ptr(s, 99));
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "slice_test");
}
