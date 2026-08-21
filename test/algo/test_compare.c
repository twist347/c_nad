#include "nad/algo/compare.h"

#include "support/pair.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// equality that deliberately disagrees with memcmp
static bool eq_abs_i32(const void *a, const void *b) {
    const int32_t x = *(const int32_t *) a;
    const int32_t y = *(const int32_t *) b;
    const int32_t ax = x < 0 ? -x : x;
    const int32_t ay = y < 0 ? -y : y;

    return nad_eq_i32(&ax, &ay);
}

// a comparator returning the raw difference, not -1/0/+1: nad_span_cmp
// must normalize it, and a test that only checks the sign would not notice
static int cmp_diff_i32(const void *a, const void *b) {
    return *(const int32_t *) a - *(const int32_t *) b;
}

static size_t eq_calls = 0;

static bool eq_counting_i32(const void *a, const void *b) {
    ++eq_calls;
    return nad_eq_i32(a, b);
}

/* ========== eq ========== */

static void test_eq_same_contents() {
    constexpr int32_t a[3] = {1, 2, 3};
    constexpr int32_t b[3] = {1, 2, 3};

    TEST_ASSERT_TRUE(nad_span_eq(
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, b, 3)
    ));
}

static void test_eq_differs_in_the_last_elem() {
    constexpr int32_t a[3] = {1, 2, 3};
    constexpr int32_t b[3] = {1, 2, 4};

    TEST_ASSERT_FALSE(nad_span_eq(
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, b, 3)
    ));
}

// length is compared before contents — a prefix is not equal to the whole
static void test_eq_different_lengths() {
    constexpr int32_t a[3] = {1, 2, 3};

    TEST_ASSERT_FALSE(nad_span_eq(
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, a, 2)
    ));
}

static void test_eq_empty_spans_are_equal() {
    constexpr int32_t a[2] = {1, 2};
    constexpr int32_t b[2] = {3, 4};

    TEST_ASSERT_TRUE(nad_span_eq(
        NAD_SPAN_NEW(int32_t, a, 0),
        NAD_SPAN_NEW(int32_t, b, 0)
    ));
}

// two null empty views must compare equal without dereferencing anything
static void test_eq_null_views_are_equal() {
    TEST_ASSERT_TRUE(nad_span_eq(
        NAD_SPAN_NEW(int32_t, nullptr, 0),
        NAD_SPAN_NEW(int32_t, nullptr, 0)
    ));
}

static void test_eq_same_buffer_is_equal() {
    constexpr int32_t a[3] = {1, 2, 3};
    const nad_Span s = NAD_SPAN_NEW(int32_t, a, 3);

    TEST_ASSERT_TRUE(nad_span_eq(s, s));
}

static void test_eq_compares_whole_elements() {
    const Pair x[2] = {{1, 2}, {3, 4}};
    const Pair y[2] = {{1, 2}, {3, 5}};

    TEST_ASSERT_FALSE(nad_span_eq(
        NAD_SPAN_NEW(Pair, x, 2),
        NAD_SPAN_NEW(Pair, y, 2)
    ));
}

/* ========== eq_by ========== */

static void test_eq_by_uses_the_predicate() {
    constexpr int32_t a[3] = {1, -2, 3};
    constexpr int32_t b[3] = {-1, 2, -3};

    // byte-wise these differ, by absolute value they match
    TEST_ASSERT_FALSE(nad_span_eq(
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, b, 3)
    ));
    TEST_ASSERT_TRUE(nad_span_eq_by(
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, b, 3),
        eq_abs_i32
    ));
}

static void test_eq_by_different_lengths() {
    constexpr int32_t a[3] = {1, 2, 3};

    TEST_ASSERT_FALSE(nad_span_eq_by(
        NAD_SPAN_NEW(int32_t, a, 3),
        NAD_SPAN_NEW(int32_t, a, 2),
        nad_eq_i32
    ));
}

// the same buffer short-circuits: the predicate must not be consulted at all
static void test_eq_by_same_buffer_skips_the_predicate() {
    constexpr int32_t a[3] = {1, 2, 3};
    const nad_Span s = NAD_SPAN_NEW(int32_t, a, 3);

    eq_calls = 0;
    TEST_ASSERT_TRUE(nad_span_eq_by(s, s, eq_counting_i32));
    TEST_ASSERT_EQUAL_size_t(0, eq_calls);
}

// a mismatch stops the walk — later elements are never examined
static void test_eq_by_stops_at_the_first_mismatch() {
    constexpr int32_t a[4] = {1, 9, 3, 4};
    constexpr int32_t b[4] = {1, 0, 3, 4};

    eq_calls = 0;
    TEST_ASSERT_FALSE(nad_span_eq_by(
        NAD_SPAN_NEW(int32_t, a, 4),
        NAD_SPAN_NEW(int32_t, b, 4),
        eq_counting_i32
    ));
    TEST_ASSERT_EQUAL_size_t(2, eq_calls);
}

static void test_eq_by_empty_spans_are_equal() {
    constexpr int32_t a[2] = {1, 2};

    eq_calls = 0;
    TEST_ASSERT_TRUE(nad_span_eq_by(
        NAD_SPAN_NEW(int32_t, a, 0),
        NAD_SPAN_NEW(int32_t, a, 0),
        eq_counting_i32
    ));
    TEST_ASSERT_EQUAL_size_t(0, eq_calls);
}

/* ========== mismatch ========== */

static void test_mismatch_reports_the_first_disagreement() {
    constexpr int32_t a[4] = {1, 2, 3, 4};
    constexpr int32_t b[4] = {1, 2, 9, 4};

    size_t idx = 999;
    TEST_ASSERT_TRUE(nad_span_mismatch(NAD_SPAN_NEW(int32_t, a, 4), NAD_SPAN_NEW(int32_t, b, 4),
                                       nad_eq_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(2, idx);
}

// index 0 is the case a "found something" flag can hide: a truthy index
// happens to double as the flag everywhere else
static void test_mismatch_at_the_very_first_elem() {
    constexpr int32_t a[3] = {9, 2, 3};
    constexpr int32_t b[3] = {1, 2, 3};

    size_t idx = 999;
    TEST_ASSERT_TRUE(nad_span_mismatch(NAD_SPAN_NEW(int32_t, a, 3), NAD_SPAN_NEW(int32_t, b, 3),
                                       nad_eq_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(0, idx);
}

static void test_mismatch_of_equal_spans_leaves_the_out_param_alone() {
    constexpr int32_t a[3] = {1, 2, 3};
    constexpr int32_t b[3] = {1, 2, 3};

    size_t idx = 777;
    TEST_ASSERT_FALSE(nad_span_mismatch(NAD_SPAN_NEW(int32_t, a, 3), NAD_SPAN_NEW(int32_t, b, 3),
                                        nad_eq_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(777, idx);
}

// only the common prefix is compared: a length difference is not a
// disagreeing elem, and eq/cmp are what answer that question
static void test_mismatch_looks_at_the_common_prefix_only() {
    constexpr int32_t shorter[2] = {1, 2};
    constexpr int32_t longer[4] = {1, 2, 3, 4};

    size_t idx = 555;
    TEST_ASSERT_FALSE(nad_span_mismatch(NAD_SPAN_NEW(int32_t, shorter, 2), NAD_SPAN_NEW(int32_t, longer, 4),
                                        nad_eq_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(555, idx);
}

static void test_mismatch_inside_the_prefix_of_unequal_lengths() {
    constexpr int32_t shorter[2] = {1, 9};
    constexpr int32_t longer[4] = {1, 2, 3, 4};

    size_t idx = 999;
    TEST_ASSERT_TRUE(nad_span_mismatch(NAD_SPAN_NEW(int32_t, shorter, 2), NAD_SPAN_NEW(int32_t, longer, 4),
                                       nad_eq_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(1, idx);
}

static void test_mismatch_of_empty_spans_finds_nothing() {
    constexpr int32_t buf[2] = {1, 2};

    size_t idx = 333;
    TEST_ASSERT_FALSE(nad_span_mismatch(NAD_SPAN_NEW(int32_t, nullptr, 0), NAD_SPAN_NEW(int32_t, buf, 2),
                                        nad_eq_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(333, idx);
}

static void test_mismatch_honours_the_callback() {
    constexpr int32_t a[3] = {1, -2, 3};
    constexpr int32_t b[3] = {1, 2, 4};

    size_t idx = 999;
    // -2 and 2 differ by value but agree by absolute value
    TEST_ASSERT_TRUE(nad_span_mismatch(NAD_SPAN_NEW(int32_t, a, 3), NAD_SPAN_NEW(int32_t, b, 3),
                                       eq_abs_i32, &idx));
    TEST_ASSERT_EQUAL_size_t(2, idx);
}

/* ========== cmp ========== */

static void test_cmp_of_equal_spans_is_zero() {
    constexpr int32_t a[3] = {1, 2, 3};
    constexpr int32_t b[3] = {1, 2, 3};

    TEST_ASSERT_EQUAL_INT(0, nad_span_cmp(NAD_SPAN_NEW(int32_t, a, 3), NAD_SPAN_NEW(int32_t, b, 3),
                                          nad_cmp_i32));
}

static void test_cmp_is_decided_by_the_first_difference() {
    constexpr int32_t a[3] = {1, 2, 9};
    constexpr int32_t b[3] = {1, 3, 0};

    TEST_ASSERT_EQUAL_INT(-1, nad_span_cmp(NAD_SPAN_NEW(int32_t, a, 3), NAD_SPAN_NEW(int32_t, b, 3),
                                           nad_cmp_i32));
    TEST_ASSERT_EQUAL_INT(1, nad_span_cmp(NAD_SPAN_NEW(int32_t, b, 3), NAD_SPAN_NEW(int32_t, a, 3),
                                          nad_cmp_i32));
}

// a prefix loses to the longer span it is a prefix of
static void test_cmp_orders_a_prefix_before_the_longer_span() {
    constexpr int32_t shorter[2] = {1, 2};
    constexpr int32_t longer[3] = {1, 2, 3};

    TEST_ASSERT_EQUAL_INT(-1, nad_span_cmp(NAD_SPAN_NEW(int32_t, shorter, 2), NAD_SPAN_NEW(int32_t, longer, 3),
                                           nad_cmp_i32));
    TEST_ASSERT_EQUAL_INT(1, nad_span_cmp(NAD_SPAN_NEW(int32_t, longer, 3), NAD_SPAN_NEW(int32_t, shorter, 2),
                                          nad_cmp_i32));
}

static void test_cmp_of_empty_spans() {
    const nad_Span empty = NAD_SPAN_NEW(int32_t, nullptr, 0);
    constexpr int32_t buf[1] = {1};

    TEST_ASSERT_EQUAL_INT(0, nad_span_cmp(empty, empty, nad_cmp_i32));
    TEST_ASSERT_EQUAL_INT(-1, nad_span_cmp(empty, NAD_SPAN_NEW(int32_t, buf, 1), nad_cmp_i32));
    TEST_ASSERT_EQUAL_INT(1, nad_span_cmp(NAD_SPAN_NEW(int32_t, buf, 1), empty, nad_cmp_i32));
}

// the callback may return any sign-carrying int; the span-level answer is -1/0/+1
static void test_cmp_normalizes_whatever_the_callback_returns() {
    constexpr int32_t a[2] = {1, 5};
    constexpr int32_t b[2] = {1, 100};

    TEST_ASSERT_EQUAL_INT(-1, nad_span_cmp(NAD_SPAN_NEW(int32_t, a, 2), NAD_SPAN_NEW(int32_t, b, 2),
                                           cmp_diff_i32));
    TEST_ASSERT_EQUAL_INT(1, nad_span_cmp(NAD_SPAN_NEW(int32_t, b, 2), NAD_SPAN_NEW(int32_t, a, 2),
                                          cmp_diff_i32));
}

// a descending comparator flips the order, so cmp must follow it and not the values
static void test_cmp_follows_the_comparator() {
    constexpr int32_t a[2] = {1, 2};
    constexpr int32_t b[2] = {1, 3};

    TEST_ASSERT_EQUAL_INT(1, nad_span_cmp(NAD_SPAN_NEW(int32_t, a, 2), NAD_SPAN_NEW(int32_t, b, 2),
                                          nad_cmp_desc_i32));
}

// the two agree: cmp says zero exactly when eq says true
static void test_cmp_and_eq_agree() {
    constexpr int32_t a[3] = {1, 2, 3};
    constexpr int32_t b[3] = {1, 2, 3};
    constexpr int32_t c[3] = {1, 2, 4};

    const nad_Span sa = NAD_SPAN_NEW(int32_t, a, 3);
    const nad_Span sb = NAD_SPAN_NEW(int32_t, b, 3);
    const nad_Span sc = NAD_SPAN_NEW(int32_t, c, 3);

    TEST_ASSERT_TRUE(nad_span_eq(sa, sb) == (nad_span_cmp(sa, sb, nad_cmp_i32) == 0));
    TEST_ASSERT_TRUE(nad_span_eq(sa, sc) == (nad_span_cmp(sa, sc, nad_cmp_i32) == 0));
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_eq_same_contents);
    RUN_TEST(test_eq_differs_in_the_last_elem);
    RUN_TEST(test_eq_different_lengths);
    RUN_TEST(test_eq_empty_spans_are_equal);
    RUN_TEST(test_eq_null_views_are_equal);
    RUN_TEST(test_eq_same_buffer_is_equal);
    RUN_TEST(test_eq_compares_whole_elements);

    RUN_TEST(test_eq_by_uses_the_predicate);
    RUN_TEST(test_eq_by_different_lengths);
    RUN_TEST(test_eq_by_same_buffer_skips_the_predicate);
    RUN_TEST(test_eq_by_stops_at_the_first_mismatch);
    RUN_TEST(test_eq_by_empty_spans_are_equal);

    RUN_TEST(test_mismatch_reports_the_first_disagreement);
    RUN_TEST(test_mismatch_at_the_very_first_elem);
    RUN_TEST(test_mismatch_of_equal_spans_leaves_the_out_param_alone);
    RUN_TEST(test_mismatch_looks_at_the_common_prefix_only);
    RUN_TEST(test_mismatch_inside_the_prefix_of_unequal_lengths);
    RUN_TEST(test_mismatch_of_empty_spans_finds_nothing);
    RUN_TEST(test_mismatch_honours_the_callback);

    RUN_TEST(test_cmp_of_equal_spans_is_zero);
    RUN_TEST(test_cmp_is_decided_by_the_first_difference);
    RUN_TEST(test_cmp_orders_a_prefix_before_the_longer_span);
    RUN_TEST(test_cmp_of_empty_spans);
    RUN_TEST(test_cmp_normalizes_whatever_the_callback_returns);
    RUN_TEST(test_cmp_follows_the_comparator);
    RUN_TEST(test_cmp_and_eq_agree);

    return UNITY_END();
}
