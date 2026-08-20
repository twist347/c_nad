#include "nad/core/cmp.h"
#include "nad/algo/sort.h"
#include "nad/algo/search.h"

#include "unity.h"

#include <math.h>
#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// The value form the public API no longer carries. It lives here, in the only file that
// wants readable operands, and is exactly what nad_cmp_<T> used to be: the comparator
// called with the addresses of two values. Instantiated only where a case calls it, so
// an unused one is a warning rather than dead weight.
#define DEFINE_CMP_FORM(name, T)      \
    static int cmp_##name(T a, T b) { \
        return nad_cmp_##name(&a, &b); \
    }

#define DEFINE_EQ_FORM(name, T)       \
    static bool eq_##name(T a, T b) { \
        return nad_eq_##name(&a, &b);  \
    }

DEFINE_CMP_FORM(i8, int8_t)
DEFINE_CMP_FORM(i16, int16_t)
DEFINE_CMP_FORM(i32, int32_t)
DEFINE_CMP_FORM(i64, int64_t)
DEFINE_CMP_FORM(u8, uint8_t)
DEFINE_CMP_FORM(u16, uint16_t)
DEFINE_CMP_FORM(u32, uint32_t)
DEFINE_CMP_FORM(u64, uint64_t)
DEFINE_CMP_FORM(size, size_t)
DEFINE_CMP_FORM(f32, float)
DEFINE_CMP_FORM(f64, double)
DEFINE_CMP_FORM(cstr, const char *)

DEFINE_EQ_FORM(i32, int32_t)
DEFINE_EQ_FORM(u8, uint8_t)
DEFINE_EQ_FORM(size, size_t)
DEFINE_EQ_FORM(f64, double)
DEFINE_EQ_FORM(cstr, const char *)

#undef DEFINE_CMP_FORM
#undef DEFINE_EQ_FORM

// every comparator must answer with a sign, not just any int of the right side
static void expect_order(int less, int equal, int greater) {
    TEST_ASSERT_EQUAL_INT(-1, less);
    TEST_ASSERT_EQUAL_INT(0, equal);
    TEST_ASSERT_EQUAL_INT(1, greater);
}

/* ========== signed ========== */

static void test_cmp_signed_orders_and_normalizes() {
    expect_order(cmp_i8(-1, 0), cmp_i8(7, 7), cmp_i8(0, -1));
    expect_order(cmp_i16(-1, 0), cmp_i16(7, 7), cmp_i16(0, -1));
    expect_order(cmp_i32(-1, 0), cmp_i32(7, 7), cmp_i32(0, -1));
    expect_order(cmp_i64(-1, 0), cmp_i64(7, 7), cmp_i64(0, -1));
}

// the extremes are where a subtracting comparator would overflow and answer backwards
static void test_cmp_signed_survives_the_extremes() {
    expect_order(
        cmp_i8(INT8_MIN, INT8_MAX),
        cmp_i8(INT8_MIN, INT8_MIN),
        cmp_i8(INT8_MAX, INT8_MIN)
    );
    expect_order(
        cmp_i32(INT32_MIN, INT32_MAX),
        cmp_i32(INT32_MIN, INT32_MIN),
        cmp_i32(INT32_MAX, INT32_MIN)
    );
    expect_order(
        cmp_i64(INT64_MIN, INT64_MAX),
        cmp_i64(INT64_MIN, INT64_MIN),
        cmp_i64(INT64_MAX, INT64_MIN)
    );
}

/* ========== unsigned ========== */

static void test_cmp_unsigned_orders_and_normalizes() {
    expect_order(cmp_u8(0, 1), cmp_u8(7, 7), cmp_u8(1, 0));
    expect_order(cmp_u16(0, 1), cmp_u16(7, 7), cmp_u16(1, 0));
    expect_order(cmp_u32(0, 1), cmp_u32(7, 7), cmp_u32(1, 0));
    expect_order(cmp_u64(0, 1), cmp_u64(7, 7), cmp_u64(1, 0));
    expect_order(cmp_size(0, 1), cmp_size(7, 7), cmp_size(1, 0));
}

// unsigned has no negatives to wrap into, but a subtracting comparator still wraps
static void test_cmp_unsigned_survives_the_extremes() {
    expect_order(cmp_u8(0, UINT8_MAX), cmp_u8(0, 0), cmp_u8(UINT8_MAX, 0));
    expect_order(cmp_u64(0, UINT64_MAX), cmp_u64(0, 0), cmp_u64(UINT64_MAX, 0));
    expect_order(cmp_size(0, SIZE_MAX), cmp_size(0, 0), cmp_size(SIZE_MAX, 0));
}

/* ========== floats ========== */

static void test_cmp_float_orders_ordinary_values() {
    expect_order(cmp_f32(1.0f, 2.0f), cmp_f32(2.0f, 2.0f), cmp_f32(2.0f, 1.0f));
    expect_order(cmp_f64(1.0, 2.0), cmp_f64(2.0, 2.0), cmp_f64(2.0, 1.0));
}

// IEEE says -0.0 == +0.0, and the ordering must agree rather than invent a tie-break
static void test_cmp_float_treats_the_two_zeroes_as_one() {
    TEST_ASSERT_EQUAL_INT(0, cmp_f32(-0.0f, 0.0f));
    TEST_ASSERT_EQUAL_INT(0, cmp_f64(-0.0, 0.0));
    TEST_ASSERT_TRUE(eq_f64(-0.0, 0.0));
}

// the reason these are not (a > b) - (a < b): every comparison against NaN is false,
// so the naive form calls NaN equal to everything and the order stops being an order
static void test_cmp_float_sorts_nan_after_every_number() {
    const double nan_val = NAN;

    TEST_ASSERT_EQUAL_INT(1, cmp_f64(nan_val, 0.0));
    TEST_ASSERT_EQUAL_INT(1, cmp_f64(nan_val, INFINITY));
    TEST_ASSERT_EQUAL_INT(-1, cmp_f64(-INFINITY, nan_val));
    TEST_ASSERT_EQUAL_INT(0, cmp_f64(nan_val, nan_val));

    TEST_ASSERT_TRUE(eq_f64(nan_val, nan_val));
    TEST_ASSERT_FALSE(eq_f64(nan_val, 0.0));
}

static void test_cmp_float_orders_the_infinities() {
    expect_order(
        cmp_f64(-INFINITY, INFINITY),
        cmp_f64(INFINITY, INFINITY),
        cmp_f64(INFINITY, -INFINITY)
    );
}

// a span with a NaN in it must still come out ordered, with the NaN at the end
static void test_cmp_float_gives_sort_a_usable_order() {
    double buf[5] = {3.0, NAN, 1.0, -0.0, 2.0};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(double, buf, 5);

    nad_span_sort(s, nad_cmp_f64);

    TEST_ASSERT_TRUE(nad_span_is_sorted(nad_span_from_mut(s), nad_cmp_f64));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, buf[0]);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, buf[1]);
    TEST_ASSERT_EQUAL_DOUBLE(2.0, buf[2]);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, buf[3]);
    TEST_ASSERT_TRUE(isnan(buf[4]));
}

/* ========== cstr ========== */

static void test_cmp_cstr_orders_lexicographically() {
    expect_order(cmp_cstr("abc", "abd"), cmp_cstr("abc", "abc"), cmp_cstr("abd", "abc"));
}

// strcmp only promises a sign, and glibc happily returns values far from ±1
static void test_cmp_cstr_normalizes_to_a_sign() {
    TEST_ASSERT_EQUAL_INT(-1, cmp_cstr("a", "z"));
    TEST_ASSERT_EQUAL_INT(1, cmp_cstr("z", "a"));
}

static void test_cmp_cstr_puts_null_first() {
    TEST_ASSERT_EQUAL_INT(-1, cmp_cstr(nullptr, ""));
    TEST_ASSERT_EQUAL_INT(1, cmp_cstr("", nullptr));
    TEST_ASSERT_EQUAL_INT(0, cmp_cstr(nullptr, nullptr));
    TEST_ASSERT_TRUE(eq_cstr(nullptr, nullptr));
    TEST_ASSERT_FALSE(eq_cstr(nullptr, ""));
}

static void test_cmp_cstr_treats_a_prefix_as_smaller() {
    TEST_ASSERT_EQUAL_INT(-1, cmp_cstr("ab", "abc"));
    TEST_ASSERT_EQUAL_INT(1, cmp_cstr("abc", "ab"));
}

/* ========== equality ========== */

static void test_eq_answers_true_only_on_equal() {
    TEST_ASSERT_TRUE(eq_i32(7, 7));
    TEST_ASSERT_FALSE(eq_i32(7, 8));
    TEST_ASSERT_TRUE(eq_u8(200, 200));
    TEST_ASSERT_FALSE(eq_u8(200, 201));
    TEST_ASSERT_TRUE(eq_size(SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_TRUE(eq_cstr("abc", "abc"));
    TEST_ASSERT_FALSE(eq_cstr("abc", "abd"));
}

/* ========== descending ========== */

// the descending form is the ascending one with its operands the other way round
static void test_descending_inverts_the_ascending_one() {
    const int32_t a = 1;
    const int32_t b = 2;

    TEST_ASSERT_EQUAL_INT(1, nad_cmp_desc_i32(&a, &b));
    TEST_ASSERT_EQUAL_INT(-1, nad_cmp_desc_i32(&b, &a));
    TEST_ASSERT_EQUAL_INT(0, nad_cmp_desc_i32(&a, &a));

    const double x = 1.5;
    const double y = 2.5;
    TEST_ASSERT_EQUAL_INT(1, nad_cmp_desc_f64(&x, &y));
    TEST_ASSERT_EQUAL_INT(-1, nad_cmp_desc_f64(&y, &x));

    // cstr goes through the same shared body despite its pointer-to-pointer operands
    const char *p = "abc";
    const char *q = "abd";
    TEST_ASSERT_EQUAL_INT(1, nad_cmp_desc_cstr(&p, &q));
    TEST_ASSERT_EQUAL_INT(-1, nad_cmp_desc_cstr(&q, &p));
}

/* ========== through the algorithms ========== */

static void test_comparators_drive_sort_both_ways() {
    int32_t buf[5] = {3, 1, 5, 2, 4};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    nad_span_sort(s, nad_cmp_i32);
    constexpr int32_t up[5] = {1, 2, 3, 4, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(up, buf, 5);

    nad_span_sort(s, nad_cmp_desc_i32);
    constexpr int32_t down[5] = {5, 4, 3, 2, 1};
    TEST_ASSERT_EQUAL_INT32_ARRAY(down, buf, 5);
}

static void test_cstr_comparator_drives_sort() {
    const char *buf[4] = {"pear", "apple", nullptr, "fig"};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(const char *, buf, 4);

    nad_span_sort(s, nad_cmp_cstr);

    TEST_ASSERT_NULL(buf[0]); // null sorts first
    TEST_ASSERT_EQUAL_STRING("apple", buf[1]);
    TEST_ASSERT_EQUAL_STRING("fig", buf[2]);
    TEST_ASSERT_EQUAL_STRING("pear", buf[3]);
}

static void test_eq_drives_search() {
    constexpr int32_t buf[5] = {10, 20, 30, 20, 10};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 5);

    size_t idx = 0;
    TEST_ASSERT_TRUE(nad_span_find(s, &(int32_t){20}, nad_eq_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(1, idx);
    TEST_ASSERT_EQUAL_size_t(2, nad_span_count(s, &(int32_t){20}, nad_eq_i32));
    TEST_ASSERT_FALSE(nad_span_contains(s, &(int32_t){99}, nad_eq_i32));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_cmp_signed_orders_and_normalizes);
    RUN_TEST(test_cmp_signed_survives_the_extremes);

    RUN_TEST(test_cmp_unsigned_orders_and_normalizes);
    RUN_TEST(test_cmp_unsigned_survives_the_extremes);

    RUN_TEST(test_cmp_float_orders_ordinary_values);
    RUN_TEST(test_cmp_float_treats_the_two_zeroes_as_one);
    RUN_TEST(test_cmp_float_sorts_nan_after_every_number);
    RUN_TEST(test_cmp_float_orders_the_infinities);
    RUN_TEST(test_cmp_float_gives_sort_a_usable_order);

    RUN_TEST(test_cmp_cstr_orders_lexicographically);
    RUN_TEST(test_cmp_cstr_normalizes_to_a_sign);
    RUN_TEST(test_cmp_cstr_puts_null_first);
    RUN_TEST(test_cmp_cstr_treats_a_prefix_as_smaller);

    RUN_TEST(test_eq_answers_true_only_on_equal);

    RUN_TEST(test_descending_inverts_the_ascending_one);

    RUN_TEST(test_comparators_drive_sort_both_ways);
    RUN_TEST(test_cstr_comparator_drives_sort);
    RUN_TEST(test_eq_drives_search);

    return UNITY_END();
}
