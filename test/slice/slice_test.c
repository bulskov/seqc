#include <criterion/criterion.h>

#include "slice/slice.h"

Test(slice, get_first_element) {
  int data[] = {10, 20, 30};
  Slice s = {data, 3, sizeof(int)};
  cr_assert_eq(*(int *)slice_get(s, 0), 10);
}

Test(slice, get_last_element) {
  int data[] = {10, 20, 30};
  Slice s = {data, 3, sizeof(int)};
  cr_assert_eq(*(int *)slice_get(s, 2), 30);
}

Test(slice, get_middle_element) {
  double data[] = {1.1, 2.2, 3.3};
  Slice s = {data, 3, sizeof(double)};
  cr_assert_float_eq(*(double *)slice_get(s, 1), 2.2, 1e-9);
}
