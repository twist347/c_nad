#include "nad/algo/copy.h"

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

/* ========== copy ========== */

static void test_copy_transfers_every_elem() {
    constexpr int32_t src_buf[4] = {1, 2, 3, 4};
    int32_t dst_buf[4] = {0, 0, 0, 0};

    nad_span_copy(
        NAD_SPAN_NEW_MUT(int32_t, dst_buf, 4),
        NAD_SPAN_NEW(int32_t, src_buf, 4)
    );

    TEST_ASSERT_EQUAL_INT32_ARRAY(src_buf, dst_buf, 4);
}

static void test_copy_empty_is_noop() {
    constexpr int32_t src_buf[2] = {7, 8};
    int32_t dst_buf[2] = {1, 2};

    nad_span_copy(
        NAD_SPAN_NEW_MUT(int32_t, dst_buf, 0),
        NAD_SPAN_NEW(int32_t, src_buf, 0)
    );

    constexpr int32_t expected[2] = {1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, dst_buf, 2);
}

// same buffer on both sides is a defined no-op, not an aliasing violation
static void test_copy_onto_itself_is_noop() {
    int32_t buf[3] = {1, 2, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    nad_span_copy(s, nad_span_from_mut(s));

    constexpr int32_t expected[3] = {1, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 3);
}

static void test_copy_stays_within_the_subspan() {
    constexpr int32_t src_buf[2] = {7, 8};
    int32_t dst_buf[4] = {0, 0, 0, 0};
    const nad_SpanMut dst = NAD_SPAN_NEW_MUT(int32_t, dst_buf, 4);

    nad_span_copy(
        nad_span_sub_mut(dst, 1, 2),
        NAD_SPAN_NEW(int32_t, src_buf, 2)
    );

    constexpr int32_t expected[4] = {0, 7, 8, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, dst_buf, 4);
}

// elem_size drives the copy, so a type wider than a word must arrive whole
static void test_copy_moves_whole_elements() {
    const Pair src_buf[2] = {{1, 2}, {3, 4}};
    Pair dst_buf[2] = {{0, 0}, {0, 0}};

    nad_span_copy(
        NAD_SPAN_NEW_MUT(Pair, dst_buf, 2),
        NAD_SPAN_NEW(Pair, src_buf, 2)
    );

    TEST_ASSERT_EQUAL_INT64(1, dst_buf[0].a);
    TEST_ASSERT_EQUAL_INT64(2, dst_buf[0].b);
    TEST_ASSERT_EQUAL_INT64(3, dst_buf[1].a);
    TEST_ASSERT_EQUAL_INT64(4, dst_buf[1].b);
}

/* ========== copy_within ========== */

// shifting right overlaps: a plain memcpy would smear the first element
static void test_copy_within_shifts_right() {
    int32_t buf[5] = {1, 2, 3, 4, 5};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    // buf[2..4] <- buf[1..3]
    nad_span_copy_within(
        nad_span_sub_mut(s, 2, 3),
        nad_span_from_mut(nad_span_sub_mut(s, 1, 3))
    );

    constexpr int32_t expected[5] = {1, 2, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

static void test_copy_within_shifts_left() {
    int32_t buf[5] = {1, 2, 3, 4, 5};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 5);

    // buf[0..2] <- buf[2..4]
    nad_span_copy_within(
        nad_span_sub_mut(s, 0, 3),
        nad_span_from_mut(nad_span_sub_mut(s, 2, 3))
    );

    constexpr int32_t expected[5] = {3, 4, 5, 4, 5};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 5);
}

static void test_copy_within_onto_itself_is_noop() {
    int32_t buf[3] = {1, 2, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    nad_span_copy_within(s, nad_span_from_mut(s));

    constexpr int32_t expected[3] = {1, 2, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 3);
}

// disjoint ranges must behave exactly like nad_span_copy
static void test_copy_within_handles_disjoint_ranges() {
    constexpr int32_t src_buf[2] = {7, 8};
    int32_t dst_buf[2] = {0, 0};

    nad_span_copy_within(
        NAD_SPAN_NEW_MUT(int32_t, dst_buf, 2),
        NAD_SPAN_NEW(int32_t, src_buf, 2)
    );

    TEST_ASSERT_EQUAL_INT32_ARRAY(src_buf, dst_buf, 2);
}

static void test_copy_within_empty_is_noop() {
    int32_t buf[2] = {1, 2};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 0);

    nad_span_copy_within(s, nad_span_from_mut(s));

    constexpr int32_t expected[2] = {1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 2);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_copy_transfers_every_elem);
    RUN_TEST(test_copy_empty_is_noop);
    RUN_TEST(test_copy_onto_itself_is_noop);
    RUN_TEST(test_copy_stays_within_the_subspan);
    RUN_TEST(test_copy_moves_whole_elements);

    RUN_TEST(test_copy_within_shifts_right);
    RUN_TEST(test_copy_within_shifts_left);
    RUN_TEST(test_copy_within_onto_itself_is_noop);
    RUN_TEST(test_copy_within_handles_disjoint_ranges);
    RUN_TEST(test_copy_within_empty_is_noop);

    return UNITY_END();
}
