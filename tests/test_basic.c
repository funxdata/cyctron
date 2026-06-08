#include <criterion/criterion.h>

Test(basic, add) {
    cr_assert_eq(1 + 1, 2, "1 + 1 should be 2");
}
