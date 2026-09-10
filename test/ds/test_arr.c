#include "trs/ds/arr.h"
#include "trs/alloc/arena.h"
#include "trs/alloc/default.h"
#include "trs/core/print.h"
#include "trs/core/util.h"

#include "support/arena.h"
#include "support/pair.h"
#include "support/probe.h"
#include "support/status.h"

#include <unity.h>

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// int32_t array holding 0, 1, ... len-1
static trs_Arr *make_arr(size_t len) {
    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_NEW_LEN(int32_t, len, trs_al_default(), &a));

    for (size_t i = 0; i < len; ++i) {
        TRS_ARR_SET(int32_t, a, i, (int32_t) i);
    }
    return a;
}

/* ========== lifetime ========== */

static void test_new_sets_shape_and_zeroes() {
    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_NEW_LEN(int32_t, 4, trs_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(4, trs_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_arr_elem_size(a));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_arr_al(a));

    constexpr int32_t zeroes[4] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(zeroes, trs_arr_data(a), 4);

    trs_arr_drop(a);
}

static void test_new_empty_has_no_buffer() {
    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_NEW_LEN(int32_t, 0, trs_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(0, trs_arr_len(a));
    TEST_ASSERT_NULL(trs_arr_data(a));

    trs_arr_drop(a);
}

static void test_drop_null_is_noop() {
    trs_arr_drop(nullptr);
}

/* ========== from_data ========== */

static void test_from_data_copies_the_source() {
    constexpr int32_t src[4] = {5, 6, 7, 8};

    trs_Arr *a = nullptr;
    TRS_TEST_OK(trs_arr_from_data(src, 4, sizeof(int32_t), trs_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(4, trs_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_arr_elem_size(a));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, trs_arr_data(a), 4);
    TEST_ASSERT_TRUE((const void *) src != trs_arr_data(a));

    trs_arr_drop(a);
}

// the array owns a copy, it does not view the source
static void test_from_data_is_detached_from_the_source() {
    int32_t src[3] = {1, 2, 3};

    trs_Arr *a = nullptr;
    TRS_TEST_OK(trs_arr_from_data(src, 3, sizeof(int32_t), trs_al_default(), &a));

    src[0] = 999;
    TEST_ASSERT_EQUAL_INT32(1, *TRS_ARR_GET_AS(int32_t, a, 0));

    trs_arr_drop(a);
}

// null source is legal while len == 0 — same rule as trs_span_from_data
static void test_from_data_empty_has_no_buffer() {
    trs_Arr *a = nullptr;
    TRS_TEST_OK(trs_arr_from_data(nullptr, 0, sizeof(int32_t), trs_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(0, trs_arr_len(a));
    TEST_ASSERT_NULL(trs_arr_data(a));

    trs_arr_drop(a);
}

// elem_size drives the copy, so a type wider than a word must arrive whole
static void test_from_data_copies_whole_elements() {
    constexpr Pair src[2] = {{1, 2}, {3, 4}};

    trs_Arr *arr = nullptr;
    TRS_TEST_OK(trs_arr_from_data(src, 2, sizeof(Pair), trs_al_default(), &arr));

    const Pair *got = trs_arr_data(arr);
    TEST_ASSERT_EQUAL_INT64(1, got[0].a);
    TEST_ASSERT_EQUAL_INT64(2, got[0].b);
    TEST_ASSERT_EQUAL_INT64(3, got[1].a);
    TEST_ASSERT_EQUAL_INT64(4, got[1].b);

    trs_arr_drop(arr);
}

/* ========== access ========== */

static void test_set_get_roundtrip() {
    trs_Arr *a = make_arr(5);

    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT32((int32_t) i, *TRS_ARR_GET_AS(int32_t, a, i));
    }

    trs_arr_drop(a);
}

static void test_get_mut_writes_through() {
    trs_Arr *a = make_arr(3);

    *TRS_ARR_GET_MUT_AS(int32_t, a, 1) = 42;
    TEST_ASSERT_EQUAL_INT32(42, *TRS_ARR_GET_AS(int32_t, a, 1));

    trs_arr_drop(a);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    trs_Arr *src = make_arr(4);

    trs_Arr *dst = nullptr;
    TRS_TEST_OK(trs_arr_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(4, trs_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(trs_arr_data(src), trs_arr_data(dst), 4);
    TEST_ASSERT_TRUE(trs_arr_data(src) != trs_arr_data(dst));

    TRS_ARR_SET(int32_t, src, 0, 999);
    TEST_ASSERT_EQUAL_INT32(0, *TRS_ARR_GET_AS(int32_t, dst, 0));

    trs_arr_drop(dst);
    trs_arr_drop(src);
}

static void test_copy_assign_grow_shrink_empty() {
    trs_Arr *src = make_arr(6);
    trs_Arr *dst = make_arr(2);

    // grow: 2 -> 6
    TRS_TEST_OK(trs_arr_copy_assign(src, dst));
    TEST_ASSERT_EQUAL_size_t(6, trs_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(trs_arr_data(src), trs_arr_data(dst), 6);

    // shrink: 6 -> 3
    trs_Arr *small = make_arr(3);
    TRS_TEST_OK(trs_arr_copy_assign(small, dst));
    TEST_ASSERT_EQUAL_size_t(3, trs_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(trs_arr_data(small), trs_arr_data(dst), 3);

    // shrink to empty: buffer must be released, not kept
    trs_Arr *empty = make_arr(0);
    TRS_TEST_OK(trs_arr_copy_assign(empty, dst));
    TEST_ASSERT_EQUAL_size_t(0, trs_arr_len(dst));
    TEST_ASSERT_NULL(trs_arr_data(dst));

    trs_arr_drop(empty);
    trs_arr_drop(small);
    trs_arr_drop(dst);
    trs_arr_drop(src);
}

static void test_copy_assign_self_is_noop() {
    trs_Arr *a = make_arr(3);

    TRS_TEST_OK(trs_arr_copy_assign(a, a));
    TEST_ASSERT_EQUAL_size_t(3, trs_arr_len(a));
    TEST_ASSERT_EQUAL_INT32(2, *TRS_ARR_GET_AS(int32_t, a, 2));

    trs_arr_drop(a);
}

/* ========== mods / views ========== */

static void test_swap_exchanges_contents() {
    trs_Arr *a = make_arr(2);
    trs_Arr *b = make_arr(5);

    trs_arr_swap(a, b);

    TEST_ASSERT_EQUAL_size_t(5, trs_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(2, trs_arr_len(b));
    TEST_ASSERT_EQUAL_INT32(4, *TRS_ARR_GET_AS(int32_t, a, 4));

    trs_arr_drop(b);
    trs_arr_drop(a);
}

static void test_span_views_the_same_memory() {
    trs_Arr *a = make_arr(4);

    const trs_SpanMut s = trs_arr_to_span_mut(a);
    TEST_ASSERT_EQUAL_PTR(trs_arr_data(a), s.data);
    TEST_ASSERT_EQUAL_size_t(4, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);

    TRS_SPAN_SET(int32_t, s, 0, 77);
    TEST_ASSERT_EQUAL_INT32(77, *TRS_ARR_GET_AS(int32_t, a, 0));

    trs_arr_drop(a);
}

/* ========== from_span ========== */

static void test_from_span_copies_the_view() {
    constexpr int32_t src[3] = {7, 8, 9};
    const trs_Span s = TRS_SPAN_FROM_DATA(int32_t, src, 3);

    trs_Arr *a = nullptr;
    TRS_TEST_OK(trs_arr_from_span(s, trs_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(3, trs_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_arr_elem_size(a));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, trs_arr_data(a), 3);
    TEST_ASSERT_TRUE((const void *) src != trs_arr_data(a));

    trs_arr_drop(a);
}

static void test_from_span_empty_has_no_buffer() {
    const trs_Span s = TRS_SPAN_FROM_DATA(int32_t, nullptr, 0);

    trs_Arr *a = nullptr;
    TRS_TEST_OK(trs_arr_from_span(s, trs_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(0, trs_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_arr_elem_size(a));
    TEST_ASSERT_NULL(trs_arr_data(a));

    trs_arr_drop(a);
}

// arr -> span -> arr must round-trip without touching the original
static void test_from_span_of_an_arr_round_trips() {
    trs_Arr *src = make_arr(4);

    trs_Arr *dst = nullptr;
    TRS_TEST_OK(trs_arr_from_span(trs_arr_to_span(src), trs_al_default(), &dst));

    TEST_ASSERT_EQUAL_size_t(4, trs_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(trs_arr_data(src), trs_arr_data(dst), 4);
    TEST_ASSERT_TRUE(trs_arr_data(src) != trs_arr_data(dst));

    trs_arr_drop(dst);
    trs_arr_drop(src);
}

/* ========== first / last ========== */

static void test_first_and_last_address_the_ends() {
    trs_Arr *a = make_arr(4);

    TEST_ASSERT_EQUAL_INT32(0, *TRS_ARR_FRONT_AS(int32_t, a));
    TEST_ASSERT_EQUAL_INT32(3, *TRS_ARR_BACK_AS(int32_t, a));
    TEST_ASSERT_EQUAL_PTR(trs_arr_data(a), trs_arr_front(a));
    TEST_ASSERT_EQUAL_PTR(TRS_ARR_GET_AS(int32_t, a, 3), trs_arr_back(a));

    trs_arr_drop(a);
}

static void test_first_and_last_mut_write_through() {
    trs_Arr *a = make_arr(4);

    *TRS_ARR_FRONT_MUT_AS(int32_t, a) = 10;
    *TRS_ARR_BACK_MUT_AS(int32_t, a) = 20;

    TEST_ASSERT_EQUAL_INT32(10, *TRS_ARR_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(20, *TRS_ARR_GET_AS(int32_t, a, 3));

    trs_arr_drop(a);
}

static void test_first_and_last_coincide_on_a_single_elem() {
    trs_Arr *a = make_arr(1);

    TEST_ASSERT_EQUAL_PTR(trs_arr_front(a), trs_arr_back(a));
    TEST_ASSERT_EQUAL_PTR(trs_arr_front_mut(a), trs_arr_back_mut(a));

    trs_arr_drop(a);
}

/* ========== data_mut / foreach ========== */

static void test_data_mut_writes_through() {
    trs_Arr *a = make_arr(3);

    int32_t *d = trs_arr_data_mut(a);
    d[2] = 99;

    TEST_ASSERT_EQUAL_INT32(99, *TRS_ARR_GET_AS(int32_t, a, 2));

    trs_arr_drop(a);
}

/* ========== swap_elems ========== */

static void test_swap_elems_exchanges_the_pair() {
    trs_Arr *a = make_arr(4);

    trs_arr_swap_elems(a, 0, 3);

    TEST_ASSERT_EQUAL_INT32(3, *TRS_ARR_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(0, *TRS_ARR_GET_AS(int32_t, a, 3));
    TEST_ASSERT_EQUAL_INT32(1, *TRS_ARR_GET_AS(int32_t, a, 1));
    TEST_ASSERT_EQUAL_INT32(2, *TRS_ARR_GET_AS(int32_t, a, 2));

    trs_arr_drop(a);
}

static void test_swap_elems_same_index_is_noop() {
    trs_Arr *a = make_arr(3);

    trs_arr_swap_elems(a, 1, 1);

    TEST_ASSERT_EQUAL_INT32(1, *TRS_ARR_GET_AS(int32_t, a, 1));

    trs_arr_drop(a);
}

// elem_size drives the swap, so a type wider than a word must move whole
static void test_swap_elems_moves_wide_elems_whole() {
    constexpr Pair src[2] = {{1, 2}, {3, 4}};

    trs_Arr *arr = nullptr;
    TRS_TEST_OK(TRS_ARR_FROM_DATA(Pair, src, 2, trs_al_default(), &arr));

    trs_arr_swap_elems(arr, 0, 1);

    TEST_ASSERT_EQUAL_INT64(3, TRS_ARR_GET_AS(Pair, arr, 0)->a);
    TEST_ASSERT_EQUAL_INT64(4, TRS_ARR_GET_AS(Pair, arr, 0)->b);
    TEST_ASSERT_EQUAL_INT64(1, TRS_ARR_GET_AS(Pair, arr, 1)->a);
    TEST_ASSERT_EQUAL_INT64(2, TRS_ARR_GET_AS(Pair, arr, 1)->b);

    trs_arr_drop(arr);
}

/* ========== swap ========== */

static void test_swap_self_is_noop() {
    trs_Arr *a = make_arr(3);
    const void *before = trs_arr_data(a);

    trs_arr_swap(a, a);

    TEST_ASSERT_EQUAL_PTR(before, trs_arr_data(a));
    TEST_ASSERT_EQUAL_size_t(3, trs_arr_len(a));
    TEST_ASSERT_EQUAL_INT32(2, *TRS_ARR_GET_AS(int32_t, a, 2));

    trs_arr_drop(a);
}

// one allocator on both sides: the buffers are handed over, never copied
static void test_swap_same_allocator_hands_over_buffers() {
    trs_Arr *a = make_arr(2);
    trs_Arr *b = make_arr(5);

    const void *pa = trs_arr_data(a);
    const void *pb = trs_arr_data(b);

    trs_arr_swap(a, b);

    TEST_ASSERT_EQUAL_PTR(pb, trs_arr_data(a));
    TEST_ASSERT_EQUAL_PTR(pa, trs_arr_data(b));

    trs_arr_drop(b);
    trs_arr_drop(a);
}

/* ========== to span ========== */

static void test_to_span_matches_the_arr_shape() {
    trs_Arr *a = make_arr(4);

    const trs_Span s = trs_arr_to_span(a);

    TEST_ASSERT_EQUAL_PTR(trs_arr_data(a), s.data);
    TEST_ASSERT_EQUAL_size_t(4, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);
    TEST_ASSERT_EQUAL_INT32(2, *TRS_SPAN_GET_AS(int32_t, s, 2));

    trs_arr_drop(a);
}

static void test_to_span_of_empty_keeps_elem_size() {
    trs_Arr *a = make_arr(0);

    const trs_Span s = trs_arr_to_span(a);

    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL_size_t(0, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);

    trs_arr_drop(a);
}

/* ========== allocators ========== */

static void test_copy_inherits_the_source_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *src = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, arena, &src, 1, 2, 3));

    trs_Arr *dst = nullptr;
    TRS_TEST_OK(trs_arr_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_arr_al(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(trs_arr_data(src), trs_arr_data(dst), 3);

    trs_arr_drop(dst);
    trs_arr_drop(src);
    trs_al_arena_drop(arena);
}

static void test_copy_with_builds_on_the_given_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *src = make_arr(4);

    trs_Arr *dst = nullptr;
    TRS_TEST_OK(trs_arr_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_arr_al(dst));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_arr_al(src));
    TEST_ASSERT_TRUE(trs_arr_eq(src, dst));

    // the source is gone and the copy still holds the elems: they were taken, not viewed
    trs_arr_drop(src);
    TEST_ASSERT_EQUAL_INT32(3, *TRS_ARR_GET_AS(int32_t, dst, 3));

    trs_arr_drop(dst);
    trs_al_arena_drop(arena);
}

// the blocks are asked of the allocator the copy is going to, not of the source's
static void test_copy_with_reports_an_exhausted_target_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_Arr *src = make_arr(4);

    trs_Arr *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_arr_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(4, trs_arr_len(src));

    trs_arr_drop(src);
    trs_al_arena_drop(arena);
}

static void test_move_assign_hands_over_the_contents_on_one_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Arr *src = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, &al, &src, 1, 2, 3));

    trs_Arr *dst = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, &al, &dst, 9));

    const size_t requests = trs_test_probe_requests(&probe);
    TRS_TEST_OK(trs_arr_move_assign(src, dst));

    // nothing was asked of the allocator: the block changed hands as it stood
    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    TEST_ASSERT_EQUAL_size_t(3, trs_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32(2, *TRS_ARR_GET_AS(int32_t, dst, 1));

    // the source is left empty and usable, not dangling
    TEST_ASSERT_EQUAL_size_t(0, trs_arr_len(src));
    TEST_ASSERT_NULL(trs_arr_data(src));

    trs_arr_drop(src);
    trs_arr_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_move_assign_across_allocators_empties_the_source() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *src = make_arr(4);

    trs_Arr *dst = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, arena, &dst, 9));

    TRS_TEST_OK(trs_arr_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(4, trs_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32(3, *TRS_ARR_GET_AS(int32_t, dst, 3));
    TEST_ASSERT_EQUAL_PTR(arena, trs_arr_al(dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_arr_len(src));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_arr_al(src));

    trs_arr_drop(src);
    trs_arr_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_across_allocators_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *dst = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, arena, &dst, 9));
    trs_test_arena_leave(arena, 0);

    trs_Arr *src = make_arr(4);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_arr_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(4, trs_arr_len(src));
    TEST_ASSERT_EQUAL_size_t(1, trs_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32(9, *TRS_ARR_GET_AS(int32_t, dst, 0));

    trs_arr_drop(src);
    trs_arr_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_of_itself_changes_nothing() {
    trs_Arr *a = make_arr(3);

    TRS_TEST_OK(trs_arr_move_assign(a, a));

    TEST_ASSERT_EQUAL_size_t(3, trs_arr_len(a));
    TEST_ASSERT_EQUAL_INT32(2, *TRS_ARR_GET_AS(int32_t, a, 2));

    trs_arr_drop(a);
}

// assignment resizes through the target's allocator, not the source's
static void test_copy_assign_keeps_the_target_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *src = make_arr(4);

    trs_Arr *dst = nullptr;
    TRS_TEST_OK(TRS_ARR_NEW_LEN(int32_t, 1, arena, &dst));

    TRS_TEST_OK(trs_arr_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(4, trs_arr_len(dst));
    TEST_ASSERT_EQUAL_PTR(arena, trs_arr_al(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(trs_arr_data(src), trs_arr_data(dst), 4);

    trs_arr_drop(dst);
    trs_arr_drop(src);
    trs_al_arena_drop(arena);
}

/* ========== allocation failure ========== */

// len * elem_size overflows size_t: reported, never attempted
static void test_new_len_reports_size_overflow() {
    trs_Arr *a = nullptr;

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_arr_new_len(SIZE_MAX, 2, trs_al_default(), &a));

    TEST_ASSERT_NULL(a); // out is untouched on failure
}

static void test_from_data_reports_size_overflow() {
    constexpr int32_t src[1] = {1};
    trs_Arr *a = nullptr;

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_arr_from_data(src, SIZE_MAX, 2, trs_al_default(), &a));

    TEST_ASSERT_NULL(a);
}

static void test_new_len_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *a = nullptr;

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_ARR_NEW_LEN(int32_t, 1000, arena, &a));

    TEST_ASSERT_NULL(a);

    trs_al_arena_drop(arena);
}

static void test_from_data_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 128);
    TEST_ASSERT_NOT_NULL(arena);

    constexpr int32_t src[4] = {1, 2, 3, 4};
    trs_Arr *a = nullptr;

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_arr_from_data(src, 1000, sizeof(int32_t), arena, &a));

    TEST_ASSERT_NULL(a);

    trs_al_arena_drop(arena);
}

// a copy asks the SOURCE's allocator for both blocks, so an exhausted arena under the
// source is what refuses it
static void test_copy_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *src = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, arena, &src, 1, 2, 3));
    trs_test_arena_leave(arena, 0);

    trs_Arr *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_arr_copy(src, &dst));

    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(3, trs_arr_len(src)); // the source is only read

    trs_al_arena_drop(arena);
}

// the header alone is refused: the buffer is never asked for, and 'out' stays untouched
static void test_copy_of_empty_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 128);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *src = nullptr;
    TRS_TEST_OK(TRS_ARR_NEW_LEN(int32_t, 0, arena, &src));
    trs_test_arena_leave(arena, 0);

    trs_Arr *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_arr_copy(src, &dst));

    TEST_ASSERT_NULL(dst);

    trs_al_arena_drop(arena);
}

// A copy_assign that changes the length has to resize the TARGET's buffer, which is the
// only allocation this operation makes. When it is refused the target must be left whole
// — the old length, the old block and the old elems — rather than half converted
static void test_copy_assign_reports_an_exhausted_arena_and_changes_nothing() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Arr *other = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, arena, &other, 7, 8));

    trs_Arr *self = make_arr(8); // default allocator, 0 .. 7

    const void *before = trs_arr_data(other);
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_arr_copy_assign(self, other));

    TEST_ASSERT_EQUAL_size_t(2, trs_arr_len(other));
    TEST_ASSERT_EQUAL_PTR(before, trs_arr_data(other));
    TEST_ASSERT_EQUAL_INT32(7, *TRS_ARR_GET_AS(int32_t, other, 0));
    TEST_ASSERT_EQUAL_INT32(8, *TRS_ARR_GET_AS(int32_t, other, 1));

    trs_arr_drop(self);
    trs_al_arena_drop(arena);
}

// equal lengths need no new room, so the elems are written over the block the target
// already has. The probe is what makes "no allocation" checkable at all
static void test_copy_assign_of_the_same_length_never_allocates() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Arr *self = nullptr;
    trs_Arr *other = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, &al, &self, 1, 2, 3));
    TRS_TEST_OK(TRS_ARR_OF(int32_t, &al, &other, 9, 9, 9));

    const size_t requests = trs_test_probe_requests(&probe);
    const void *before = trs_arr_data(other);

    TRS_TEST_OK(trs_arr_copy_assign(self, other));

    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_PTR(before, trs_arr_data(other));
    TEST_ASSERT_EQUAL_INT32(1, *TRS_ARR_GET_AS(int32_t, other, 0));
    TEST_ASSERT_EQUAL_INT32(3, *TRS_ARR_GET_AS(int32_t, other, 2));

    trs_arr_drop(self);
    trs_arr_drop(other);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// The arr is built in two allocations, the header first and then the buffer. When the
// second is refused the first must not be stranded: the probe counts what is still live,
// and an arena would hide the leak because it frees everything at once
static void test_a_refused_buffer_frees_the_header() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 1);

    trs_Arr *a = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_ARR_NEW_LEN(int32_t, 4, &al, &a));

    TEST_ASSERT_NULL(a);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the same for the filled constructor, which takes its buffer with trs_alloc rather than
// trs_calloc — a different call, the same rule
static void test_a_refused_buffer_frees_the_header_of_from_data() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 1);

    trs_Arr *a = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_ARR_OF(int32_t, &al, &a, 1, 2, 3));

    TEST_ASSERT_NULL(a);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// two blocks go into a filled arr and drop must hand back both. The default allocator
// would say nothing about it, so the count comes from a probe
static void test_drop_hands_back_everything_it_took() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, &al, &a, 1, 2, 3, 4));
    TEST_ASSERT_EQUAL_size_t(2, probe.live);

    trs_arr_drop(a);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
    TEST_ASSERT_EQUAL_size_t(2, probe.dealloc_calls);
}

// an empty arr owns a header and nothing else, so drop hands back exactly one block
static void test_drop_of_empty_hands_back_the_header_alone() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_NEW_LEN(int32_t, 0, &al, &a));
    TEST_ASSERT_EQUAL_size_t(1, probe.live);

    trs_arr_drop(a);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
    TEST_ASSERT_EQUAL_size_t(1, probe.dealloc_calls);
}

/* ========== macros ========== */

static void test_macro_of_builds_from_literals() {
    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, trs_al_default(), &a, 4, 5, 6));

    TEST_ASSERT_EQUAL_size_t(3, trs_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_arr_elem_size(a));

    constexpr int32_t want[3] = {4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, trs_arr_data(a), 3);

    trs_arr_drop(a);
}

static void test_macro_of_derives_len_from_the_list() {
    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int64_t, trs_al_default(), &a, 1, 2, 3, 4, 5));

    TEST_ASSERT_EQUAL_size_t(5, trs_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int64_t), trs_arr_elem_size(a));

    trs_arr_drop(a);
}

static void test_macro_from_data_infers_elem_size() {
    constexpr int32_t src[2] = {1, 2};

    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_FROM_DATA(int32_t, src, 2, trs_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(2, trs_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_arr_elem_size(a));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, trs_arr_data(a), 2);

    trs_arr_drop(a);
}

/* ========== bytes ========== */

static void test_bytes_is_len_times_elem_size() {
    trs_Arr *a = make_arr(4);

    TEST_ASSERT_EQUAL_size_t(4 * sizeof(int32_t), trs_arr_bytes(a));

    trs_arr_drop(a);
}

static void test_bytes_of_empty_is_zero() {
    trs_Arr *a = make_arr(0);

    TEST_ASSERT_EQUAL_size_t(0, trs_arr_bytes(a));

    trs_arr_drop(a);
}

// elem_size, not the elem count, drives the total
static void test_bytes_tracks_elem_size() {
    constexpr Pair src[2] = {{1, 2}, {3, 4}};

    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_FROM_DATA(Pair, src, 2, trs_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(2 * sizeof(Pair), trs_arr_bytes(a));

    trs_arr_drop(a);
}

// the arr and its view must agree on the size of the same memory
static void test_bytes_agrees_with_the_span() {
    trs_Arr *a = make_arr(3);

    TEST_ASSERT_EQUAL_size_t(trs_span_bytes(trs_arr_to_span(a)), trs_arr_bytes(a));

    trs_arr_drop(a);
}

/* ========== compare ========== */

static void test_eq_matches_the_same_elems() {
    trs_Arr *a = make_arr(4);
    trs_Arr *b = make_arr(4);

    TEST_ASSERT_TRUE(trs_arr_eq(a, a));
    TEST_ASSERT_TRUE(trs_arr_eq(a, b));
    TEST_ASSERT_TRUE(trs_arr_eq(b, a));
    TEST_ASSERT_TRUE(trs_arr_eq_by(a, b, trs_eq_i32));

    trs_arr_drop(a);
    trs_arr_drop(b);
}

static void test_eq_parts_one_differing_elem() {
    trs_Arr *a = make_arr(4);
    trs_Arr *b = make_arr(4);
    TRS_ARR_SET(int32_t, b, 3, 99);

    TEST_ASSERT_FALSE(trs_arr_eq(a, b));
    TEST_ASSERT_FALSE(trs_arr_eq(b, a));
    TEST_ASSERT_FALSE(trs_arr_eq_by(a, b, trs_eq_i32));

    trs_arr_drop(a);
    trs_arr_drop(b);
}

// a prefix of the other, so nothing but the length tells the two apart
static void test_eq_parts_different_lengths() {
    trs_Arr *a = make_arr(4);
    trs_Arr *shorter = make_arr(3);

    TEST_ASSERT_FALSE(trs_arr_eq(a, shorter));
    TEST_ASSERT_FALSE(trs_arr_eq(shorter, a));
    TEST_ASSERT_FALSE(trs_arr_eq_by(a, shorter, trs_eq_i32));

    trs_arr_drop(a);
    trs_arr_drop(shorter);
}

static void test_eq_of_two_empties() {
    trs_Arr *a = make_arr(0);
    trs_Arr *b = make_arr(0);
    trs_Arr *one = make_arr(1);

    TEST_ASSERT_TRUE(trs_arr_eq(a, b));
    TEST_ASSERT_TRUE(trs_arr_eq_by(a, b, trs_eq_i32));
    TEST_ASSERT_FALSE(trs_arr_eq(a, one));
    TEST_ASSERT_FALSE(trs_arr_eq(one, a));

    trs_arr_drop(a);
    trs_arr_drop(b);
    trs_arr_drop(one);
}

// the equality decides, and it can see less than the bytes do: these Pairs agree in the
// first field and differ in the second
static void test_eq_by_asks_the_equality() {
    constexpr Pair lhs[2] = {{1, 10}, {2, 20}};
    constexpr Pair rhs[2] = {{1, 70}, {2, 80}};

    trs_Arr *a = nullptr;
    trs_Arr *b = nullptr;
    TRS_TEST_OK(TRS_ARR_FROM_DATA(Pair, lhs, 2, trs_al_default(), &a));
    TRS_TEST_OK(TRS_ARR_FROM_DATA(Pair, rhs, 2, trs_al_default(), &b));

    TEST_ASSERT_FALSE(trs_arr_eq(a, b));
    TEST_ASSERT_TRUE(trs_arr_eq_by(a, b, trs_test_pair_eq_a));

    trs_arr_drop(a);
    trs_arr_drop(b);
}

/* ========== print ========== */

// a printer writes to a stream, so a case has to read one back. tmpfile is the portable
// way, the same one test/core/test_print.c takes
static void assert_prints(const char *expected, const trs_Arr *a) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);

    trs_arr_fprint(a, stream, trs_fprint_i32);
    rewind(stream);

    char buf[128];
    const size_t n = fread(buf, 1, sizeof buf - 1, stream);
    buf[n] = '\0';
    fclose(stream);

    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_fprint_writes_the_elems() {
    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, trs_al_default(), &a, 5, 3, 1));

    assert_prints("[5, 3, 1]\n", a);

    trs_arr_drop(a);
}

static void test_fprint_of_a_single_elem_has_no_separator() {
    trs_Arr *a = nullptr;
    TRS_TEST_OK(TRS_ARR_OF(int32_t, trs_al_default(), &a, 7));

    assert_prints("[7]\n", a);

    trs_arr_drop(a);
}

static void test_fprint_of_an_empty_arr() {
    trs_Arr *a = make_arr(0);

    assert_prints("[]\n", a);

    trs_arr_drop(a);
}

// the stdout twin takes no stream, and C has no portable way to capture one and give it
// back — so a case can only say that it runs and reaches the same printer
static void test_print_writes_to_stdout() {
    trs_Arr *a = make_arr(3);

    trs_arr_print(a, trs_fprint_i32);

    trs_arr_drop(a);
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

    RUN_TEST(test_from_span_copies_the_view);
    RUN_TEST(test_from_span_empty_has_no_buffer);
    RUN_TEST(test_from_span_of_an_arr_round_trips);

    RUN_TEST(test_first_and_last_address_the_ends);
    RUN_TEST(test_first_and_last_mut_write_through);
    RUN_TEST(test_first_and_last_coincide_on_a_single_elem);

    RUN_TEST(test_data_mut_writes_through);

    RUN_TEST(test_swap_elems_exchanges_the_pair);
    RUN_TEST(test_swap_elems_same_index_is_noop);
    RUN_TEST(test_swap_elems_moves_wide_elems_whole);

    RUN_TEST(test_swap_self_is_noop);
    RUN_TEST(test_swap_same_allocator_hands_over_buffers);

    RUN_TEST(test_to_span_matches_the_arr_shape);
    RUN_TEST(test_to_span_of_empty_keeps_elem_size);

    RUN_TEST(test_copy_inherits_the_source_allocator);
    RUN_TEST(test_copy_with_builds_on_the_given_allocator);
    RUN_TEST(test_copy_with_reports_an_exhausted_target_arena);
    RUN_TEST(test_move_assign_hands_over_the_contents_on_one_allocator);
    RUN_TEST(test_move_assign_across_allocators_empties_the_source);
    RUN_TEST(test_move_assign_across_allocators_reports_an_exhausted_arena);
    RUN_TEST(test_move_assign_of_itself_changes_nothing);
    RUN_TEST(test_copy_assign_keeps_the_target_allocator);

    RUN_TEST(test_new_len_reports_size_overflow);
    RUN_TEST(test_from_data_reports_size_overflow);
    RUN_TEST(test_new_len_reports_an_exhausted_arena);
    RUN_TEST(test_from_data_reports_an_exhausted_arena);
    RUN_TEST(test_copy_reports_an_exhausted_arena);
    RUN_TEST(test_copy_of_empty_reports_an_exhausted_arena);
    RUN_TEST(test_copy_assign_reports_an_exhausted_arena_and_changes_nothing);
    RUN_TEST(test_copy_assign_of_the_same_length_never_allocates);
    RUN_TEST(test_a_refused_buffer_frees_the_header);
    RUN_TEST(test_a_refused_buffer_frees_the_header_of_from_data);
    RUN_TEST(test_drop_hands_back_everything_it_took);
    RUN_TEST(test_drop_of_empty_hands_back_the_header_alone);

    RUN_TEST(test_macro_of_builds_from_literals);
    RUN_TEST(test_macro_of_derives_len_from_the_list);
    RUN_TEST(test_macro_from_data_infers_elem_size);

    RUN_TEST(test_bytes_is_len_times_elem_size);
    RUN_TEST(test_bytes_of_empty_is_zero);
    RUN_TEST(test_bytes_tracks_elem_size);
    RUN_TEST(test_bytes_agrees_with_the_span);


    RUN_TEST(test_eq_matches_the_same_elems);
    RUN_TEST(test_eq_parts_one_differing_elem);
    RUN_TEST(test_eq_parts_different_lengths);
    RUN_TEST(test_eq_of_two_empties);
    RUN_TEST(test_eq_by_asks_the_equality);

    RUN_TEST(test_fprint_writes_the_elems);
    RUN_TEST(test_fprint_of_a_single_elem_has_no_separator);
    RUN_TEST(test_fprint_of_an_empty_arr);
    RUN_TEST(test_print_writes_to_stdout);

    return UNITY_END();
}
