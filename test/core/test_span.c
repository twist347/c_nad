#include "nad/core/span.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// an elem wider than a word, to keep elem_size honest
typedef struct {
    int64_t a;
    int64_t b;
} Pair;

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

static void test_mut_to_span_preserves_view() {
    const nad_SpanMut m = NAD_SPAN_OF_MUT(int32_t, 1, 2);
    const nad_Span s = nad_span_mut_to_span(m);

    TEST_ASSERT_EQUAL_PTR(m.data, s.data);
    TEST_ASSERT_EQUAL_size_t(m.len, s.len);
    TEST_ASSERT_EQUAL_size_t(m.elem_size, s.elem_size);
}

// mut_to_span must hand back a view of the same memory, not a copy
static void test_mut_to_span_shares_the_memory() {
    int32_t buf[3] = {1, 2, 3};
    const nad_SpanMut m = NAD_SPAN_NEW_MUT(int32_t, buf, 3);
    const nad_Span s = nad_span_mut_to_span(m);

    NAD_SPAN_SET(int32_t, m, 1, 99);

    TEST_ASSERT_EQUAL_INT32(99, *NAD_SPAN_GET_AS(int32_t, s, 1));
}

/* ========== of ========== */

static void test_of_views_the_literal() {
    const nad_Span s = NAD_SPAN_OF(int32_t, 3, 1, 2);

    TEST_ASSERT_EQUAL_size_t(3, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);
    TEST_ASSERT_EQUAL_INT32(3, *NAD_SPAN_GET_AS(int32_t, s, 0));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_SPAN_GET_AS(int32_t, s, 1));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_SPAN_GET_AS(int32_t, s, 2));
}

static void test_of_derives_len_from_the_list() {
    TEST_ASSERT_EQUAL_size_t(1, NAD_SPAN_OF(int32_t, 42).len);
    TEST_ASSERT_EQUAL_size_t(5, NAD_SPAN_OF(int32_t, 1, 2, 3, 4, 5).len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int64_t), NAD_SPAN_OF(int64_t, 1, 2).elem_size);
}

// a brace-enclosed elem survives the macro: the commas split the args,
// __VA_ARGS__ pastes them back
static void test_of_carries_struct_elems() {
    const nad_Span s = NAD_SPAN_OF(Pair, {1, 2}, {3, 4});

    TEST_ASSERT_EQUAL_size_t(2, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), s.elem_size);
    TEST_ASSERT_EQUAL_INT64(1, NAD_SPAN_GET_AS(Pair, s, 0)->a);
    TEST_ASSERT_EQUAL_INT64(2, NAD_SPAN_GET_AS(Pair, s, 0)->b);
    TEST_ASSERT_EQUAL_INT64(3, NAD_SPAN_GET_AS(Pair, s, 1)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_SPAN_GET_AS(Pair, s, 1)->b);
}

// the literal behind a mut view is not const, so it takes writes
static void test_of_mut_is_writable() {
    const nad_SpanMut s = NAD_SPAN_OF_MUT(int32_t, 10, 20, 30);

    NAD_SPAN_SET(int32_t, s, 0, 99);
    *NAD_SPAN_GET_MUT_AS(int32_t, s, 2) = 77;
    nad_span_swap_elems(s, 0, 2);

    const nad_Span v = nad_span_mut_to_span(s);
    TEST_ASSERT_EQUAL_INT32(77, *NAD_SPAN_GET_AS(int32_t, v, 0));
    TEST_ASSERT_EQUAL_INT32(20, *NAD_SPAN_GET_AS(int32_t, v, 1));
    TEST_ASSERT_EQUAL_INT32(99, *NAD_SPAN_GET_AS(int32_t, v, 2));
}

static void test_of_mut_carries_struct_elems() {
    const nad_SpanMut s = NAD_SPAN_OF_MUT(Pair, {1, 2}, {3, 4});

    nad_span_swap_elems(s, 0, 1);

    TEST_ASSERT_EQUAL_INT64(3, NAD_SPAN_GET_MUT_AS(Pair, s, 0)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_SPAN_GET_MUT_AS(Pair, s, 0)->b);
    TEST_ASSERT_EQUAL_INT64(1, NAD_SPAN_GET_MUT_AS(Pair, s, 1)->a);
    TEST_ASSERT_EQUAL_INT64(2, NAD_SPAN_GET_MUT_AS(Pair, s, 1)->b);
}

/* ========== info ========== */

static void test_bytes() {
    constexpr int32_t buf[4] = {0, 0, 0, 0};

    TEST_ASSERT_EQUAL_size_t(4 * sizeof(int32_t), nad_span_bytes(NAD_SPAN_NEW(int32_t, buf, 4)));
    TEST_ASSERT_EQUAL_size_t(0, nad_span_bytes(NAD_SPAN_NEW(int32_t, buf, 0)));
}

static void test_bytes_of_null_view_is_zero() {
    TEST_ASSERT_EQUAL_size_t(0, nad_span_bytes(NAD_SPAN_NEW(int32_t, nullptr, 0)));
}

// elem_size, not the elem count, drives the total
static void test_bytes_tracks_elem_size() {
    TEST_ASSERT_EQUAL_size_t(2 * sizeof(Pair), nad_span_bytes(NAD_SPAN_OF(Pair, {1, 2}, {3, 4})));
}

/* ========== access ========== */

static void test_get_reads_through() {
    const nad_Span s = NAD_SPAN_OF(int32_t, 10, 20, 30);

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

// the two bounds sub allows: the whole span, and the exact tail
static void test_sub_takes_the_whole_span_and_the_exact_tail() {
    const nad_Span s = NAD_SPAN_OF(int32_t, 0, 1, 2, 3);

    const nad_Span all = nad_span_sub(s, 0, 4);
    TEST_ASSERT_EQUAL_PTR(s.data, all.data);
    TEST_ASSERT_EQUAL_size_t(4, all.len);

    // start > 0 and start + count == len
    const nad_Span tail = nad_span_sub(s, 2, 2);
    TEST_ASSERT_EQUAL_size_t(2, tail.len);
    TEST_ASSERT_EQUAL_INT32(2, *NAD_SPAN_GET_AS(int32_t, tail, 0));
    TEST_ASSERT_EQUAL_INT32(3, *NAD_SPAN_GET_AS(int32_t, tail, 1));
}

// start == len is legal and yields an empty view, not an error
static void test_sub_at_end_is_empty() {
    const nad_Span s = NAD_SPAN_OF(int32_t, 0, 1, 2);

    const nad_Span tail = nad_span_sub(s, 3, 0);
    TEST_ASSERT_EQUAL_size_t(0, tail.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), tail.elem_size);
}

// the offset is start * elem_size, so a wide elem must land on its boundary
static void test_sub_offsets_by_elem_size() {
    const nad_Span s = NAD_SPAN_OF(Pair, {1, 2}, {3, 4}, {5, 6});

    const nad_Span tail = nad_span_sub(s, 1, 2);
    TEST_ASSERT_EQUAL_size_t(2, tail.len);
    TEST_ASSERT_EQUAL_INT64(3, NAD_SPAN_GET_AS(Pair, tail, 0)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_SPAN_GET_AS(Pair, tail, 0)->b);
    TEST_ASSERT_EQUAL_INT64(5, NAD_SPAN_GET_AS(Pair, tail, 1)->a);
    TEST_ASSERT_EQUAL_INT64(6, NAD_SPAN_GET_AS(Pair, tail, 1)->b);
}

// a null view must survive subspanning without forming a null + offset pointer
static void test_sub_of_null_view_stays_null() {
    const nad_Span s = NAD_SPAN_NEW(int32_t, nullptr, 0);

    const nad_Span sub = nad_span_sub(s, 0, 0);
    TEST_ASSERT_NULL(sub.data);
    TEST_ASSERT_EQUAL_size_t(0, sub.len);
}

// the mirror of the case above, for the mut branch
static void test_sub_mut_of_null_view_stays_null() {
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, nullptr, 0);

    const nad_SpanMut sub = nad_span_sub_mut(s, 0, 0);
    TEST_ASSERT_NULL(sub.data);
    TEST_ASSERT_EQUAL_size_t(0, sub.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), sub.elem_size);
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
    RUN_TEST(test_mut_to_span_preserves_view);
    RUN_TEST(test_mut_to_span_shares_the_memory);

    RUN_TEST(test_of_views_the_literal);
    RUN_TEST(test_of_derives_len_from_the_list);
    RUN_TEST(test_of_carries_struct_elems);
    RUN_TEST(test_of_mut_is_writable);
    RUN_TEST(test_of_mut_carries_struct_elems);

    RUN_TEST(test_bytes);
    RUN_TEST(test_bytes_of_null_view_is_zero);
    RUN_TEST(test_bytes_tracks_elem_size);

    RUN_TEST(test_get_reads_through);
    RUN_TEST(test_set_and_get_mut_write_to_the_source);

    RUN_TEST(test_sub_offsets_and_shortens);
    RUN_TEST(test_sub_takes_the_whole_span_and_the_exact_tail);
    RUN_TEST(test_sub_at_end_is_empty);
    RUN_TEST(test_sub_offsets_by_elem_size);
    RUN_TEST(test_sub_of_null_view_stays_null);
    RUN_TEST(test_sub_mut_of_null_view_stays_null);
    RUN_TEST(test_sub_mut_writes_reach_the_source);

    RUN_TEST(test_swap_elems);
    RUN_TEST(test_swap_elems_same_index_is_noop);
    RUN_TEST(test_swap_elems_moves_whole_element);

    return UNITY_END();
}
