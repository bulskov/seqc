#include <criterion/criterion.h>

#include "seqc/slice.h"

Test(slice, get_first_element)
{
    int data[] = {10, 20, 30};
    Slice s = {data, 3, sizeof(int)};
    cr_assert_eq(*(int *)slice_get(s, 0), 10);
}

Test(slice, get_last_element)
{
    int data[] = {10, 20, 30};
    Slice s = {data, 3, sizeof(int)};
    cr_assert_eq(*(int *)slice_get(s, 2), 30);
}

Test(slice, get_middle_element)
{
    double data[] = {1.1, 2.2, 3.3};
    Slice s = {data, 3, sizeof(double)};
    cr_assert_float_eq(*(double *)slice_get(s, 1), 2.2, 1e-9);
}

/* ---- slice_find / slice_contains ---------------------------------------- */

static bool int_gt_three(const void *elem, void *ctx)
{
    (void)ctx;
    return *(const int *)elem > 3;
}

Test(slice, find_returns_first_match)
{
    int data[] = {1, 2, 4, 3, 5};
    Slice s = {data, 5, sizeof(int)};
    int *p = slice_find(s, int_gt_three, NULL);
    cr_assert_not_null(p);
    cr_assert_eq(*p, 4);
}

Test(slice, find_returns_null_when_no_match)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    cr_assert_null(slice_find(s, int_gt_three, NULL));
}

Test(slice, contains_true_when_match_exists)
{
    int data[] = {1, 5, 2};
    Slice s = {data, 3, sizeof(int)};
    cr_assert(slice_contains(s, int_gt_three, NULL));
}

Test(slice, contains_false_when_no_match)
{
    int data[] = {1, 2, 3};
    Slice s = {data, 3, sizeof(int)};
    cr_assert(!slice_contains(s, int_gt_three, NULL));
}

/* slice_get with an out-of-bounds index must return NULL. */
Test(slice, get_out_of_bounds_returns_null)
{
    int data[] = {10, 20};
    Slice s = {data, 2, sizeof(int)};
    cr_assert_null(slice_get(s, 2));
    cr_assert_null(slice_get(s, 99));
}
