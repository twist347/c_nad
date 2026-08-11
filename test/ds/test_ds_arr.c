#include "nad/ds/arr.h"
#include "nad/alloc/alloc_default.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// int32_t array holding 0, 1, ... len-1
static nad_Arr *make_arr(size_t len) {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_NEW(int32_t, len, nad_al_default(), &a));

    for (size_t i = 0; i < len; ++i) {
        NAD_ARR_SET(int32_t, a, i, (int32_t) i);
    }
    return a;
}

/* ========== lifetime ========== */

static void test_new_sets_shape_and_zeroes() {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_NEW(int32_t, 4, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(4, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_arr_al(a));

    const int32_t zeroes[4] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(zeroes, nad_arr_data(a), 4);

    nad_arr_drop(a);
}

static void test_new_empty_has_no_buffer() {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_NEW(int32_t, 0, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(0, nad_arr_len(a));
    TEST_ASSERT_NULL(nad_arr_data(a));

    nad_arr_drop(a);
}

static void test_drop_null_is_noop() {
    nad_arr_drop(nullptr);
}

/* ========== from_data ========== */

static void test_from_data_copies_the_source() {
    constexpr int32_t src[4] = {5, 6, 7, 8};

    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_arr_from_data(src, 4, sizeof(int32_t), nad_al_default(), &a)
    );

    TEST_ASSERT_EQUAL_size_t(4, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, nad_arr_data(a), 4);
    TEST_ASSERT_TRUE((const void *) src != nad_arr_data(a));

    nad_arr_drop(a);
}

// the array owns a copy, it does not view the source
static void test_from_data_is_detached_from_the_source() {
    int32_t src[3] = {1, 2, 3};

    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_arr_from_data(src, 3, sizeof(int32_t), nad_al_default(), &a)
    );

    src[0] = 999;
    TEST_ASSERT_EQUAL_INT32(1, *NAD_ARR_GET_AS(int32_t, a, 0));

    nad_arr_drop(a);
}

// null source is legal while len == 0 — same rule as nad_span_new
static void test_from_data_empty_has_no_buffer() {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_arr_from_data(nullptr, 0, sizeof(int32_t), nad_al_default(), &a)
    );

    TEST_ASSERT_EQUAL_size_t(0, nad_arr_len(a));
    TEST_ASSERT_NULL(nad_arr_data(a));

    nad_arr_drop(a);
}

// elem_size drives the copy, so a type wider than a word must arrive whole
static void test_from_data_copies_whole_elements() {
    typedef struct {
        int64_t a;
        int64_t b;
    } Pair;

    const Pair src[2] = {{1, 2}, {3, 4}};

    nad_Arr *arr = nullptr;
    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OK,
        nad_arr_from_data(src, 2, sizeof(Pair), nad_al_default(), &arr)
    );

    const Pair *got = nad_arr_data(arr);
    TEST_ASSERT_EQUAL_INT64(1, got[0].a);
    TEST_ASSERT_EQUAL_INT64(2, got[0].b);
    TEST_ASSERT_EQUAL_INT64(3, got[1].a);
    TEST_ASSERT_EQUAL_INT64(4, got[1].b);

    nad_arr_drop(arr);
}

/* ========== access ========== */

static void test_set_get_roundtrip() {
    nad_Arr *a = make_arr(5);

    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT32((int32_t) i, *NAD_ARR_GET_AS(int32_t, a, i));
    }

    nad_arr_drop(a);
}

static void test_get_mut_writes_through() {
    nad_Arr *a = make_arr(3);

    *NAD_ARR_GET_MUT_AS(int32_t, a, 1) = 42;
    TEST_ASSERT_EQUAL_INT32(42, *NAD_ARR_GET_AS(int32_t, a, 1));

    nad_arr_drop(a);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    nad_Arr *src = make_arr(4);

    nad_Arr *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(4, nad_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(src), nad_arr_data(dst), 4);
    TEST_ASSERT_TRUE(nad_arr_data(src) != nad_arr_data(dst));

    NAD_ARR_SET(int32_t, src, 0, 999);
    TEST_ASSERT_EQUAL_INT32(0, *NAD_ARR_GET_AS(int32_t, dst, 0));

    nad_arr_drop(dst);
    nad_arr_drop(src);
}

static void test_copy_assign_grow_shrink_empty() {
    nad_Arr *src = make_arr(6);
    nad_Arr *dst = make_arr(2);

    // grow: 2 -> 6
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_copy_assign(src, dst));
    TEST_ASSERT_EQUAL_size_t(6, nad_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(src), nad_arr_data(dst), 6);

    // shrink: 6 -> 3
    nad_Arr *small = make_arr(3);
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_copy_assign(small, dst));
    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(small), nad_arr_data(dst), 3);

    // shrink to empty: buffer must be released, not kept
    nad_Arr *empty = make_arr(0);
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_copy_assign(empty, dst));
    TEST_ASSERT_EQUAL_size_t(0, nad_arr_len(dst));
    TEST_ASSERT_NULL(nad_arr_data(dst));

    nad_arr_drop(empty);
    nad_arr_drop(small);
    nad_arr_drop(dst);
    nad_arr_drop(src);
}

static void test_copy_assign_self_is_noop() {
    nad_Arr *a = make_arr(3);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_copy_assign(a, a));
    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_ARR_GET_AS(int32_t, a, 2));

    nad_arr_drop(a);
}

/* ========== mods / views ========== */

static void test_swap_exchanges_contents() {
    nad_Arr *a = make_arr(2);
    nad_Arr *b = make_arr(5);

    nad_arr_swap(a, b);

    TEST_ASSERT_EQUAL_size_t(5, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(2, nad_arr_len(b));
    TEST_ASSERT_EQUAL_INT32(4, *NAD_ARR_GET_AS(int32_t, a, 4));

    nad_arr_drop(b);
    nad_arr_drop(a);
}

static void test_span_views_the_same_memory() {
    nad_Arr *a = make_arr(4);

    const nad_SpanMut s = nad_arr_to_span_mut(a);
    TEST_ASSERT_EQUAL_PTR(nad_arr_data(a), s.data);
    TEST_ASSERT_EQUAL_size_t(4, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);

    NAD_SPAN_SET(int32_t, s, 0, 77);
    TEST_ASSERT_EQUAL_INT32(77, *NAD_ARR_GET_AS(int32_t, a, 0));

    nad_arr_drop(a);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_sets_shape_and_zeroes);
    RUN_TEST(test_new_empty_has_no_buffer);
    RUN_TEST(test_drop_null_is_noop);

    RUN_TEST(test_from_data_copies_the_source);
    RUN_TEST(test_from_data_is_detached_from_the_source);
    RUN_TEST(test_from_data_empty_has_no_buffer);
    RUN_TEST(test_from_data_copies_whole_elements);

    RUN_TEST(test_set_get_roundtrip);
    RUN_TEST(test_get_mut_writes_through);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_assign_grow_shrink_empty);
    RUN_TEST(test_copy_assign_self_is_noop);

    RUN_TEST(test_swap_exchanges_contents);
    RUN_TEST(test_span_views_the_same_memory);

    return UNITY_END();
}
