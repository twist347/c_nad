#include "nad/algo/fill.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

typedef struct {
    int64_t a;
    int64_t b;
} Pair;

/* ========== fill ========== */

static void test_fill_writes_every_elem() {
    int32_t buf[4] = {0, 0, 0, 0};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 4);

    constexpr int32_t val = 7;
    nad_span_fill(s, &val);

    constexpr int32_t expected[4] = {7, 7, 7, 7};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 4);
}

static void test_fill_empty_is_noop() {
    int32_t buf[2] = {1, 2};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 0);

    constexpr int32_t val = 9;
    nad_span_fill(s, &val);

    constexpr int32_t expected[2] = {1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 2);
}

// filling a subspan must stay inside it — the neighbours are not part of the view
static void test_fill_stays_within_the_subspan() {
    int32_t buf[5] = {0, 0, 0, 0, 0};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    constexpr int32_t val = 8;
    nad_span_fill(nad_span_sub_mut(s, 1, 3), &val);

    constexpr int32_t expected[5] = {0, 8, 8, 8, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

// elem_size drives the write, so a type wider than a word must be copied whole
static void test_fill_copies_whole_elements() {
    Pair buf[2] = {{0, 0}, {0, 0}};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(Pair, buf, 2);

    const Pair val = {11, 22};
    nad_span_fill(s, &val);

    TEST_ASSERT_EQUAL_INT64(11, buf[0].a);
    TEST_ASSERT_EQUAL_INT64(22, buf[0].b);
    TEST_ASSERT_EQUAL_INT64(11, buf[1].a);
    TEST_ASSERT_EQUAL_INT64(22, buf[1].b);
}

// the source is read once per element, so it may live inside the span itself
static void test_fill_from_an_element_of_the_same_span() {
    int32_t buf[3] = {5, 1, 2};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    nad_span_fill(s, &buf[0]);

    constexpr int32_t expected[3] = {5, 5, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 3);
}

/* ========== fill_zero ========== */

static void test_fill_zero_clears_every_byte() {
    int32_t buf[3] = {1, 2, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    nad_span_fill_zero(s);

    constexpr int32_t expected[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 3);
}

static void test_fill_zero_empty_is_noop() {
    int32_t buf[2] = {1, 2};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 0);

    nad_span_fill_zero(s);

    constexpr int32_t expected[2] = {1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 2);
}

static void test_fill_zero_stays_within_the_subspan() {
    int32_t buf[4] = {1, 2, 3, 4};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 4);

    nad_span_fill_zero(nad_span_sub_mut(s, 1, 2));

    constexpr int32_t expected[4] = {1, 0, 0, 4};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 4);
}

// an empty view over null must not reach memset with a null pointer
static void test_fill_zero_null_view_is_noop() {
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, nullptr, 0);

    nad_span_fill_zero(s);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_fill_writes_every_elem);
    RUN_TEST(test_fill_empty_is_noop);
    RUN_TEST(test_fill_stays_within_the_subspan);
    RUN_TEST(test_fill_copies_whole_elements);
    RUN_TEST(test_fill_from_an_element_of_the_same_span);

    RUN_TEST(test_fill_zero_clears_every_byte);
    RUN_TEST(test_fill_zero_empty_is_noop);
    RUN_TEST(test_fill_zero_stays_within_the_subspan);
    RUN_TEST(test_fill_zero_null_view_is_noop);

    return UNITY_END();
}
