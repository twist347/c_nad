#include "nad/algo/compare.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

static bool eq_i32(const void *a, const void *b) {
    return *(const int32_t *) a == *(const int32_t *) b;
}

// equality that deliberately disagrees with memcmp
static bool eq_abs_i32(const void *a, const void *b) {
    const int32_t x = *(const int32_t *) a;
    const int32_t y = *(const int32_t *) b;
    return (x < 0 ? -x : x) == (y < 0 ? -y : y);
}

static size_t eq_calls = 0;

static bool eq_counting_i32(const void *a, const void *b) {
    ++eq_calls;
    return eq_i32(a, b);
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
    typedef struct {
        int64_t a;
        int64_t b;
    } Pair;

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
        eq_i32
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

    return UNITY_END();
}
