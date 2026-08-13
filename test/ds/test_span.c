#include "nad/ds/span.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

/* ========== construction ========== */

static void test_new_keeps_fields() {
    constexpr int32_t buf[3] = {10, 20, 30};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 3);

    TEST_ASSERT_EQUAL_PTR(buf, s.data);
    TEST_ASSERT_EQUAL_size_t(3, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);
}

// a null view is legal only while empty — elem_size stays meaningful
static void test_new_empty_over_null() {
    const nad_Span s = NAD_SPAN_NEW(int32_t, nullptr, 0);

    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL_size_t(0, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);
}

static void test_from_mut_preserves_view() {
    int32_t buf[2] = {1, 2};
    const nad_SpanMut m = NAD_SPAN_NEW_MUT(int32_t, buf, 2);
    const nad_Span s = nad_span_from_mut(m);

    TEST_ASSERT_EQUAL_PTR(m.data, s.data);
    TEST_ASSERT_EQUAL_size_t(m.len, s.len);
    TEST_ASSERT_EQUAL_size_t(m.elem_size, s.elem_size);
}

/* ========== info ========== */

static void test_bytes() {
    constexpr int32_t buf[4] = {0, 0, 0, 0};

    TEST_ASSERT_EQUAL_size_t(4 * sizeof(int32_t), nad_span_bytes(NAD_SPAN_NEW(int32_t, buf, 4)));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_bytes(NAD_SPAN_NEW(int32_t, buf, 0)));
}

/* ========== access ========== */

static void test_get_reads_through() {
    constexpr int32_t buf[3] = {10, 20, 30};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 3);

    TEST_ASSERT_EQUAL_INT32(10, *NAD_SPAN_GET_AS(int32_t, s, 0));
    TEST_ASSERT_EQUAL_INT32(30, *NAD_SPAN_GET_AS(int32_t, s, 2));
}

static void test_set_and_get_mut_write_to_the_source() {
    int32_t buf[3] = {10, 20, 30};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    NAD_SPAN_SET(int32_t, s, 1, 99);
    *NAD_SPAN_GET_MUT_AS(int32_t, s, 2) = 77;

    // the span borrows buf, so the writes must be visible there
    constexpr int32_t expected[3] = {10, 99, 77};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 3);
}

/* ========== subspan ========== */

static void test_sub_offsets_and_shortens() {
    const int32_t buf[5] = {0, 1, 2, 3, 4};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 5);

    const nad_Span mid = nad_span_sub(s, 1, 3);
    TEST_ASSERT_EQUAL_PTR(&buf[1], mid.data);
    TEST_ASSERT_EQUAL_size_t(3, mid.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), mid.elem_size);
    TEST_ASSERT_EQUAL_INT32(1, *NAD_SPAN_GET_AS(int32_t, mid, 0));
    TEST_ASSERT_EQUAL_INT32(3, *NAD_SPAN_GET_AS(int32_t, mid, 2));

    // subspans compose: indices are relative to the span they are taken from
    const nad_Span inner = nad_span_sub(mid, 1, 1);
    TEST_ASSERT_EQUAL_size_t(1, inner.len);
    TEST_ASSERT_EQUAL_INT32(2, *NAD_SPAN_GET_AS(int32_t, inner, 0));
}

// start == len is legal and yields an empty view, not an error
static void test_sub_at_end_is_empty() {
    constexpr int32_t buf[3] = {0, 1, 2};
    const nad_Span s = NAD_SPAN_NEW(int32_t, buf, 3);

    const nad_Span tail = nad_span_sub(s, 3, 0);
    TEST_ASSERT_EQUAL_size_t(0, tail.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), tail.elem_size);
}

// a null view must survive subspanning without forming a null + offset pointer
static void test_sub_of_null_view_stays_null() {
    const nad_Span s = NAD_SPAN_NEW(int32_t, nullptr, 0);

    const nad_Span sub = nad_span_sub(s, 0, 0);
    TEST_ASSERT_NULL(sub.data);
    TEST_ASSERT_EQUAL_size_t(0, sub.len);
}

static void test_sub_mut_writes_reach_the_source() {
    int32_t buf[4] = {0, 1, 2, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 4);

    const nad_SpanMut tail = nad_span_sub_mut(s, 2, 2);
    NAD_SPAN_SET(int32_t, tail, 0, 88);

    constexpr int32_t expected[4] = {0, 1, 88, 3};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 4);
}

/* ========== mods ========== */

static void test_swap_elems() {
    int32_t buf[4] = {0, 1, 2, 3};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 4);

    nad_span_swap_elems(s, 0, 3);

    constexpr int32_t expected[4] = {3, 1, 2, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 4);
}

static void test_swap_elems_same_index_is_noop() {
    int32_t buf[3] = {0, 1, 2};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 3);

    nad_span_swap_elems(s, 1, 1);

    constexpr int32_t expected[3] = {0, 1, 2};
    TEST_ASSERT_EQUAL_INT32_ARRAY(expected, buf, 3);
}

// element size drives the copy, so a type wider than a word must swap whole
static void test_swap_elems_moves_whole_element() {
    typedef struct {
        int64_t a;
        int64_t b;
    } Pair;

    Pair buf[2] = {{1, 2}, {3, 4}};
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(Pair, buf, 2);

    nad_span_swap_elems(s, 0, 1);

    TEST_ASSERT_EQUAL_INT64(3, buf[0].a);
    TEST_ASSERT_EQUAL_INT64(4, buf[0].b);
    TEST_ASSERT_EQUAL_INT64(1, buf[1].a);
    TEST_ASSERT_EQUAL_INT64(2, buf[1].b);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_keeps_fields);
    RUN_TEST(test_new_empty_over_null);
    RUN_TEST(test_from_mut_preserves_view);

    RUN_TEST(test_bytes);

    RUN_TEST(test_get_reads_through);
    RUN_TEST(test_set_and_get_mut_write_to_the_source);

    RUN_TEST(test_sub_offsets_and_shortens);
    RUN_TEST(test_sub_at_end_is_empty);
    RUN_TEST(test_sub_of_null_view_stays_null);
    RUN_TEST(test_sub_mut_writes_reach_the_source);

    RUN_TEST(test_swap_elems);
    RUN_TEST(test_swap_elems_same_index_is_noop);
    RUN_TEST(test_swap_elems_moves_whole_element);

    return UNITY_END();
}
