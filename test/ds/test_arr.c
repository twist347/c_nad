#include "nad/ds/arr.h"
#include "nad/alloc/arena.h"
#include "nad/alloc/default.h"
#include "nad/core/util.h"

#include "support/arena.h"
#include "support/pair.h"
#include "support/probe.h"
#include "support/status.h"

#include "unity.h"

#include <stdint.h>

void setUp() {
}

void tearDown() {
}

// int32_t array holding 0, 1, ... len-1
static nad_Arr *make_arr(size_t len) {
    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_NEW_LEN(int32_t, len, nad_al_default(), &a));

    for (size_t i = 0; i < len; ++i) {
        NAD_ARR_SET(int32_t, a, i, (int32_t) i);
    }
    return a;
}

/* ========== lifetime ========== */

static void test_new_sets_shape_and_zeroes() {
    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_NEW_LEN(int32_t, 4, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(4, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_arr_al(a));

    constexpr int32_t zeroes[4] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(zeroes, nad_arr_data(a), 4);

    nad_arr_drop(a);
}

static void test_new_empty_has_no_buffer() {
    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_NEW_LEN(int32_t, 0, nad_al_default(), &a));

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
    NAD_TEST_OK(nad_arr_from_data(src, 4, sizeof(int32_t), nad_al_default(), &a));

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
    NAD_TEST_OK(nad_arr_from_data(src, 3, sizeof(int32_t), nad_al_default(), &a));

    src[0] = 999;
    TEST_ASSERT_EQUAL_INT32(1, *NAD_ARR_GET_AS(int32_t, a, 0));

    nad_arr_drop(a);
}

// null source is legal while len == 0 — same rule as nad_span_new
static void test_from_data_empty_has_no_buffer() {
    nad_Arr *a = nullptr;
    NAD_TEST_OK(nad_arr_from_data(nullptr, 0, sizeof(int32_t), nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(0, nad_arr_len(a));
    TEST_ASSERT_NULL(nad_arr_data(a));

    nad_arr_drop(a);
}

// elem_size drives the copy, so a type wider than a word must arrive whole
static void test_from_data_copies_whole_elements() {
    constexpr Pair src[2] = {{1, 2}, {3, 4}};

    nad_Arr *arr = nullptr;
    NAD_TEST_OK(nad_arr_from_data(src, 2, sizeof(Pair), nad_al_default(), &arr));

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
    NAD_TEST_OK(nad_arr_copy(src, &dst));

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
    NAD_TEST_OK(nad_arr_copy_assign(src, dst));
    TEST_ASSERT_EQUAL_size_t(6, nad_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(src), nad_arr_data(dst), 6);

    // shrink: 6 -> 3
    nad_Arr *small = make_arr(3);
    NAD_TEST_OK(nad_arr_copy_assign(small, dst));
    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(small), nad_arr_data(dst), 3);

    // shrink to empty: buffer must be released, not kept
    nad_Arr *empty = make_arr(0);
    NAD_TEST_OK(nad_arr_copy_assign(empty, dst));
    TEST_ASSERT_EQUAL_size_t(0, nad_arr_len(dst));
    TEST_ASSERT_NULL(nad_arr_data(dst));

    nad_arr_drop(empty);
    nad_arr_drop(small);
    nad_arr_drop(dst);
    nad_arr_drop(src);
}

static void test_copy_assign_self_is_noop() {
    nad_Arr *a = make_arr(3);

    NAD_TEST_OK(nad_arr_copy_assign(a, a));
    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_ARR_GET_AS(int32_t, a, 2));

    nad_arr_drop(a);
}

/* ========== mods / views ========== */

static void test_swap_exchanges_contents() {
    nad_Arr *a = make_arr(2);
    nad_Arr *b = make_arr(5);

    NAD_TEST_OK(nad_arr_swap(a, b));

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

/* ========== from_span ========== */

static void test_from_span_copies_the_view() {
    constexpr int32_t src[3] = {7, 8, 9};
    const nad_Span s = NAD_SPAN_NEW(int32_t, src, 3);

    nad_Arr *a = nullptr;
    NAD_TEST_OK(nad_arr_from_span(s, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, nad_arr_data(a), 3);
    TEST_ASSERT_TRUE((const void *) src != nad_arr_data(a));

    nad_arr_drop(a);
}

static void test_from_span_empty_has_no_buffer() {
    const nad_Span s = NAD_SPAN_NEW(int32_t, nullptr, 0);

    nad_Arr *a = nullptr;
    NAD_TEST_OK(nad_arr_from_span(s, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(0, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_NULL(nad_arr_data(a));

    nad_arr_drop(a);
}

// arr -> span -> arr must round-trip without touching the original
static void test_from_span_of_an_arr_round_trips() {
    nad_Arr *src = make_arr(4);

    nad_Arr *dst = nullptr;
    NAD_TEST_OK(nad_arr_from_span(nad_arr_to_span(src), nad_al_default(), &dst));

    TEST_ASSERT_EQUAL_size_t(4, nad_arr_len(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(src), nad_arr_data(dst), 4);
    TEST_ASSERT_TRUE(nad_arr_data(src) != nad_arr_data(dst));

    nad_arr_drop(dst);
    nad_arr_drop(src);
}

/* ========== first / last ========== */

static void test_first_and_last_address_the_ends() {
    nad_Arr *a = make_arr(4);

    TEST_ASSERT_EQUAL_INT32(0, *NAD_ARR_FIRST_AS(int32_t, a));
    TEST_ASSERT_EQUAL_INT32(3, *NAD_ARR_LAST_AS(int32_t, a));
    TEST_ASSERT_EQUAL_PTR(nad_arr_data(a), nad_arr_first(a));
    TEST_ASSERT_EQUAL_PTR(NAD_ARR_GET_AS(int32_t, a, 3), nad_arr_last(a));

    nad_arr_drop(a);
}

static void test_first_and_last_mut_write_through() {
    nad_Arr *a = make_arr(4);

    *NAD_ARR_FIRST_MUT_AS(int32_t, a) = 10;
    *NAD_ARR_LAST_MUT_AS(int32_t, a) = 20;

    TEST_ASSERT_EQUAL_INT32(10, *NAD_ARR_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(20, *NAD_ARR_GET_AS(int32_t, a, 3));

    nad_arr_drop(a);
}

static void test_first_and_last_coincide_on_a_single_elem() {
    nad_Arr *a = make_arr(1);

    TEST_ASSERT_EQUAL_PTR(nad_arr_first(a), nad_arr_last(a));
    TEST_ASSERT_EQUAL_PTR(nad_arr_first_mut(a), nad_arr_last_mut(a));

    nad_arr_drop(a);
}

/* ========== data_mut / foreach ========== */

static void test_data_mut_writes_through() {
    nad_Arr *a = make_arr(3);

    int32_t *d = nad_arr_data_mut(a);
    d[2] = 99;

    TEST_ASSERT_EQUAL_INT32(99, *NAD_ARR_GET_AS(int32_t, a, 2));

    nad_arr_drop(a);
}

/* ========== swap_elems ========== */

static void test_swap_elems_exchanges_the_pair() {
    nad_Arr *a = make_arr(4);

    nad_arr_swap_elems(a, 0, 3);

    TEST_ASSERT_EQUAL_INT32(3, *NAD_ARR_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(0, *NAD_ARR_GET_AS(int32_t, a, 3));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_ARR_GET_AS(int32_t, a, 1));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_ARR_GET_AS(int32_t, a, 2));

    nad_arr_drop(a);
}

static void test_swap_elems_same_index_is_noop() {
    nad_Arr *a = make_arr(3);

    nad_arr_swap_elems(a, 1, 1);

    TEST_ASSERT_EQUAL_INT32(1, *NAD_ARR_GET_AS(int32_t, a, 1));

    nad_arr_drop(a);
}

// elem_size drives the swap, so a type wider than a word must move whole
static void test_swap_elems_moves_wide_elems_whole() {
    constexpr Pair src[2] = {{1, 2}, {3, 4}};

    nad_Arr *arr = nullptr;
    NAD_TEST_OK(NAD_ARR_FROM_DATA(Pair, src, 2, nad_al_default(), &arr));

    nad_arr_swap_elems(arr, 0, 1);

    TEST_ASSERT_EQUAL_INT64(3, NAD_ARR_GET_AS(Pair, arr, 0)->a);
    TEST_ASSERT_EQUAL_INT64(4, NAD_ARR_GET_AS(Pair, arr, 0)->b);
    TEST_ASSERT_EQUAL_INT64(1, NAD_ARR_GET_AS(Pair, arr, 1)->a);
    TEST_ASSERT_EQUAL_INT64(2, NAD_ARR_GET_AS(Pair, arr, 1)->b);

    nad_arr_drop(arr);
}

/* ========== swap ========== */

static void test_swap_self_is_noop() {
    nad_Arr *a = make_arr(3);
    const void *before = nad_arr_data(a);

    NAD_TEST_OK(nad_arr_swap(a, a));

    TEST_ASSERT_EQUAL_PTR(before, nad_arr_data(a));
    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_ARR_GET_AS(int32_t, a, 2));

    nad_arr_drop(a);
}

// one allocator on both sides: the buffers are handed over, never copied
static void test_swap_same_allocator_hands_over_buffers() {
    nad_Arr *a = make_arr(2);
    nad_Arr *b = make_arr(5);

    const void *pa = nad_arr_data(a);
    const void *pb = nad_arr_data(b);

    NAD_TEST_OK(nad_arr_swap(a, b));

    TEST_ASSERT_EQUAL_PTR(pb, nad_arr_data(a));
    TEST_ASSERT_EQUAL_PTR(pa, nad_arr_data(b));

    nad_arr_drop(b);
    nad_arr_drop(a);
}

// two allocators: buffers cannot be handed over, the bytes are reallocated
static void test_swap_across_allocators_moves_the_bytes() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *a = make_arr(2); // default: 0, 1

    nad_Arr *b = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, arena, &b, 10, 20, 30));

    NAD_TEST_OK(nad_arr_swap(a, b));

    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(2, nad_arr_len(b));
    TEST_ASSERT_EQUAL_INT32(10, *NAD_ARR_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(30, *NAD_ARR_GET_AS(int32_t, a, 2));
    TEST_ASSERT_EQUAL_INT32(0, *NAD_ARR_GET_AS(int32_t, b, 0));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_ARR_GET_AS(int32_t, b, 1));

    // contents move, ownership does not: each array keeps its own allocator
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_arr_al(a));
    TEST_ASSERT_EQUAL_PTR(arena, nad_arr_al(b));

    nad_arr_drop(b);
    nad_arr_drop(a);
    nad_al_arena_drop(arena);
}

// the empty side allocates nothing and must end up with no buffer
static void test_swap_across_allocators_with_an_empty_side() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *a = make_arr(0);

    nad_Arr *b = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, arena, &b, 1, 2, 3));

    NAD_TEST_OK(nad_arr_swap(a, b));

    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_ARR_GET_AS(int32_t, a, 1));

    TEST_ASSERT_EQUAL_size_t(0, nad_arr_len(b));
    TEST_ASSERT_NULL(nad_arr_data(b));

    nad_arr_drop(b);
    nad_arr_drop(a);
    nad_al_arena_drop(arena);
}

// the first allocation fails: nothing has been claimed yet, so there is nothing to undo
static void test_swap_across_allocators_reports_a_failed_first_alloc() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    // a sits on the arena, so a's side is the one that allocates first
    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, arena, &a, 1, 2));

    nad_Arr *b = make_arr(3); // default: 0, 1, 2

    const void *pa = nad_arr_data(a);
    const void *pb = nad_arr_data(b);

    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_arr_swap(a, b));

    TEST_ASSERT_EQUAL_size_t(2, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(b));
    TEST_ASSERT_EQUAL_PTR(pa, nad_arr_data(a));
    TEST_ASSERT_EQUAL_PTR(pb, nad_arr_data(b));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_ARR_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_ARR_GET_AS(int32_t, b, 2));

    nad_arr_drop(b);
    nad_al_arena_drop(arena);
}

// The second allocation fails: the first must be given back and both arrays left as they
// were. The order matters for what this can observe — a's side allocates first, through
// the default allocator, so a skipped rollback leaks a real block and LeakSanitizer
// reports it. An arena on that side would swallow it, since its dealloc is a no-op.
static void test_swap_across_allocators_rolls_back_a_failed_second_alloc() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *a = make_arr(2); // default: 0, 1

    nad_Arr *b = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, arena, &b, 10, 20, 30));

    const void *pa = nad_arr_data(a);
    const void *pb = nad_arr_data(b);

    // b's side has nothing left to give
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_arr_swap(a, b));

    // both arrays untouched
    TEST_ASSERT_EQUAL_size_t(2, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(b));
    TEST_ASSERT_EQUAL_PTR(pa, nad_arr_data(a));
    TEST_ASSERT_EQUAL_PTR(pb, nad_arr_data(b));
    TEST_ASSERT_EQUAL_INT32(0, *NAD_ARR_GET_AS(int32_t, a, 0));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_ARR_GET_AS(int32_t, a, 1));
    TEST_ASSERT_EQUAL_INT32(30, *NAD_ARR_GET_AS(int32_t, b, 2));

    nad_arr_drop(a);
    nad_al_arena_drop(arena);
}

/* ========== to span ========== */

static void test_to_span_matches_the_arr_shape() {
    nad_Arr *a = make_arr(4);

    const nad_Span s = nad_arr_to_span(a);

    TEST_ASSERT_EQUAL_PTR(nad_arr_data(a), s.data);
    TEST_ASSERT_EQUAL_size_t(4, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);
    TEST_ASSERT_EQUAL_INT32(2, *NAD_SPAN_GET_AS(int32_t, s, 2));

    nad_arr_drop(a);
}

static void test_to_span_of_empty_keeps_elem_size() {
    nad_Arr *a = make_arr(0);

    const nad_Span s = nad_arr_to_span(a);

    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL_size_t(0, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);

    nad_arr_drop(a);
}

/* ========== allocators ========== */

static void test_copy_inherits_the_source_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *src = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, arena, &src, 1, 2, 3));

    nad_Arr *dst = nullptr;
    NAD_TEST_OK(nad_arr_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, nad_arr_al(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(src), nad_arr_data(dst), 3);

    nad_arr_drop(dst);
    nad_arr_drop(src);
    nad_al_arena_drop(arena);
}

static void test_copy_with_builds_on_the_given_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *src = make_arr(4);

    nad_Arr *dst = nullptr;
    NAD_TEST_OK(nad_arr_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, nad_arr_al(dst));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_arr_al(src));
    TEST_ASSERT_TRUE(nad_arr_eq(src, dst));

    // the source is gone and the copy still holds the elems: they were taken, not viewed
    nad_arr_drop(src);
    TEST_ASSERT_EQUAL_INT32(3, *NAD_ARR_GET_AS(int32_t, dst, 3));

    nad_arr_drop(dst);
    nad_al_arena_drop(arena);
}

// the blocks are asked of the allocator the copy is going to, not of the source's
static void test_copy_with_reports_an_exhausted_target_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);
    nad_test_arena_leave(arena, 0);

    nad_Arr *src = make_arr(4);

    nad_Arr *dst = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_arr_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(4, nad_arr_len(src));

    nad_arr_drop(src);
    nad_al_arena_drop(arena);
}

// assignment resizes through the target's allocator, not the source's
static void test_copy_assign_keeps_the_target_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *src = make_arr(4);

    nad_Arr *dst = nullptr;
    NAD_TEST_OK(NAD_ARR_NEW_LEN(int32_t, 1, arena, &dst));

    NAD_TEST_OK(nad_arr_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(4, nad_arr_len(dst));
    TEST_ASSERT_EQUAL_PTR(arena, nad_arr_al(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(src), nad_arr_data(dst), 4);

    nad_arr_drop(dst);
    nad_arr_drop(src);
    nad_al_arena_drop(arena);
}

/* ========== allocation failure ========== */

// len * elem_size overflows size_t: reported, never attempted
static void test_new_len_reports_size_overflow() {
    nad_Arr *a = nullptr;

    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_arr_new_len(SIZE_MAX, 2, nad_al_default(), &a));

    TEST_ASSERT_NULL(a); // out is untouched on failure
}

static void test_from_data_reports_size_overflow() {
    constexpr int32_t src[1] = {1};
    nad_Arr *a = nullptr;

    NAD_TEST_STATUS(
        NAD_STATUS_ERR_NO_MEM,
        nad_arr_from_data(src, SIZE_MAX, 2, nad_al_default(), &a)
    );

    TEST_ASSERT_NULL(a);
}

static void test_new_len_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *a = nullptr;

    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, NAD_ARR_NEW_LEN(int32_t, 1000, arena, &a));

    TEST_ASSERT_NULL(a);

    nad_al_arena_drop(arena);
}

static void test_from_data_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 128);
    TEST_ASSERT_NOT_NULL(arena);

    constexpr int32_t src[4] = {1, 2, 3, 4};
    nad_Arr *a = nullptr;

    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_arr_from_data(src, 1000, sizeof(int32_t), arena, &a));

    TEST_ASSERT_NULL(a);

    nad_al_arena_drop(arena);
}

// a copy asks the SOURCE's allocator for both blocks, so an exhausted arena under the
// source is what refuses it
static void test_copy_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *src = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, arena, &src, 1, 2, 3));
    nad_test_arena_leave(arena, 0);

    nad_Arr *dst = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_arr_copy(src, &dst));

    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(src)); // the source is only read

    nad_al_arena_drop(arena);
}

// the header alone is refused: the buffer is never asked for, and 'out' stays untouched
static void test_copy_of_empty_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 128);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *src = nullptr;
    NAD_TEST_OK(NAD_ARR_NEW_LEN(int32_t, 0, arena, &src));
    nad_test_arena_leave(arena, 0);

    nad_Arr *dst = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_arr_copy(src, &dst));

    TEST_ASSERT_NULL(dst);

    nad_al_arena_drop(arena);
}

// A copy_assign that changes the length has to resize the TARGET's buffer, which is the
// only allocation this operation makes. When it is refused the target must be left whole
// — the old length, the old block and the old elems — rather than half converted
static void test_copy_assign_reports_an_exhausted_arena_and_changes_nothing() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *other = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, arena, &other, 7, 8));

    nad_Arr *self = make_arr(8); // default allocator, 0 .. 7

    const void *before = nad_arr_data(other);
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_arr_copy_assign(self, other));

    TEST_ASSERT_EQUAL_size_t(2, nad_arr_len(other));
    TEST_ASSERT_EQUAL_PTR(before, nad_arr_data(other));
    TEST_ASSERT_EQUAL_INT32(7, *NAD_ARR_GET_AS(int32_t, other, 0));
    TEST_ASSERT_EQUAL_INT32(8, *NAD_ARR_GET_AS(int32_t, other, 1));

    nad_arr_drop(self);
    nad_al_arena_drop(arena);
}

// equal lengths need no new room, so the elems are written over the block the target
// already has. The probe is what makes "no allocation" checkable at all
static void test_copy_assign_of_the_same_length_never_allocates() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Arr *self = nullptr;
    nad_Arr *other = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, &al, &self, 1, 2, 3));
    NAD_TEST_OK(NAD_ARR_OF(int32_t, &al, &other, 9, 9, 9));

    const size_t requests = nad_test_probe_requests(&probe);
    const void *before = nad_arr_data(other);

    NAD_TEST_OK(nad_arr_copy_assign(self, other));

    TEST_ASSERT_EQUAL_size_t(requests, nad_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_PTR(before, nad_arr_data(other));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_ARR_GET_AS(int32_t, other, 0));
    TEST_ASSERT_EQUAL_INT32(3, *NAD_ARR_GET_AS(int32_t, other, 2));

    nad_arr_drop(self);
    nad_arr_drop(other);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// The arr is built in two allocations, the header first and then the buffer. When the
// second is refused the first must not be stranded: the probe counts what is still live,
// and an arena would hide the leak because it frees everything at once
static void test_a_refused_buffer_frees_the_header() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_test_probe_fail_after_next(&probe, 1);

    nad_Arr *a = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, NAD_ARR_NEW_LEN(int32_t, 4, &al, &a));

    TEST_ASSERT_NULL(a);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the same for the filled constructor, which takes its buffer with nad_alloc rather than
// nad_calloc — a different call, the same rule
static void test_a_refused_buffer_frees_the_header_of_from_data() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_test_probe_fail_after_next(&probe, 1);

    nad_Arr *a = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, NAD_ARR_OF(int32_t, &al, &a, 1, 2, 3));

    TEST_ASSERT_NULL(a);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// two blocks go into a filled arr and drop must hand back both. The default allocator
// would say nothing about it, so the count comes from a probe
static void test_drop_hands_back_everything_it_took() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, &al, &a, 1, 2, 3, 4));
    TEST_ASSERT_EQUAL_size_t(2, probe.live);

    nad_arr_drop(a);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
    TEST_ASSERT_EQUAL_size_t(2, probe.dealloc_calls);
}

// an empty arr owns a header and nothing else, so drop hands back exactly one block
static void test_drop_of_empty_hands_back_the_header_alone() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_NEW_LEN(int32_t, 0, &al, &a));
    TEST_ASSERT_EQUAL_size_t(1, probe.live);

    nad_arr_drop(a);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
    TEST_ASSERT_EQUAL_size_t(1, probe.dealloc_calls);
}

/* ========== macros ========== */

static void test_macro_of_builds_from_literals() {
    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int32_t, nad_al_default(), &a, 4, 5, 6));

    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));

    constexpr int32_t want[3] = {4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_arr_data(a), 3);

    nad_arr_drop(a);
}

static void test_macro_of_derives_len_from_the_list() {
    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_OF(int64_t, nad_al_default(), &a, 1, 2, 3, 4, 5));

    TEST_ASSERT_EQUAL_size_t(5, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int64_t), nad_arr_elem_size(a));

    nad_arr_drop(a);
}

static void test_macro_from_data_infers_elem_size() {
    constexpr int32_t src[2] = {1, 2};

    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_FROM_DATA(int32_t, src, 2, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(2, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, nad_arr_data(a), 2);

    nad_arr_drop(a);
}

/* ========== bytes ========== */

static void test_bytes_is_len_times_elem_size() {
    nad_Arr *a = make_arr(4);

    TEST_ASSERT_EQUAL_size_t(4 * sizeof(int32_t), nad_arr_bytes(a));

    nad_arr_drop(a);
}

static void test_bytes_of_empty_is_zero() {
    nad_Arr *a = make_arr(0);

    TEST_ASSERT_EQUAL_size_t(0, nad_arr_bytes(a));

    nad_arr_drop(a);
}

// elem_size, not the elem count, drives the total
static void test_bytes_tracks_elem_size() {
    constexpr Pair src[2] = {{1, 2}, {3, 4}};

    nad_Arr *a = nullptr;
    NAD_TEST_OK(NAD_ARR_FROM_DATA(Pair, src, 2, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(2 * sizeof(Pair), nad_arr_bytes(a));

    nad_arr_drop(a);
}

// the arr and its view must agree on the size of the same memory
static void test_bytes_agrees_with_the_span() {
    nad_Arr *a = make_arr(3);

    TEST_ASSERT_EQUAL_size_t(nad_span_bytes(nad_arr_to_span(a)), nad_arr_bytes(a));

    nad_arr_drop(a);
}

/* ========== compare ========== */

static void test_eq_matches_the_same_elems() {
    nad_Arr *a = make_arr(4);
    nad_Arr *b = make_arr(4);

    TEST_ASSERT_TRUE(nad_arr_eq(a, a));
    TEST_ASSERT_TRUE(nad_arr_eq(a, b));
    TEST_ASSERT_TRUE(nad_arr_eq(b, a));
    TEST_ASSERT_TRUE(nad_arr_eq_by(a, b, nad_eq_i32));

    nad_arr_drop(a);
    nad_arr_drop(b);
}

static void test_eq_parts_one_differing_elem() {
    nad_Arr *a = make_arr(4);
    nad_Arr *b = make_arr(4);
    NAD_ARR_SET(int32_t, b, 3, 99);

    TEST_ASSERT_FALSE(nad_arr_eq(a, b));
    TEST_ASSERT_FALSE(nad_arr_eq(b, a));
    TEST_ASSERT_FALSE(nad_arr_eq_by(a, b, nad_eq_i32));

    nad_arr_drop(a);
    nad_arr_drop(b);
}

// a prefix of the other, so nothing but the length tells the two apart
static void test_eq_parts_different_lengths() {
    nad_Arr *a = make_arr(4);
    nad_Arr *shorter = make_arr(3);

    TEST_ASSERT_FALSE(nad_arr_eq(a, shorter));
    TEST_ASSERT_FALSE(nad_arr_eq(shorter, a));
    TEST_ASSERT_FALSE(nad_arr_eq_by(a, shorter, nad_eq_i32));

    nad_arr_drop(a);
    nad_arr_drop(shorter);
}

static void test_eq_of_two_empties() {
    nad_Arr *a = make_arr(0);
    nad_Arr *b = make_arr(0);
    nad_Arr *one = make_arr(1);

    TEST_ASSERT_TRUE(nad_arr_eq(a, b));
    TEST_ASSERT_TRUE(nad_arr_eq_by(a, b, nad_eq_i32));
    TEST_ASSERT_FALSE(nad_arr_eq(a, one));
    TEST_ASSERT_FALSE(nad_arr_eq(one, a));

    nad_arr_drop(a);
    nad_arr_drop(b);
    nad_arr_drop(one);
}

// the equality decides, and it can see less than the bytes do: these Pairs agree in the
// first field and differ in the second
static void test_eq_by_asks_the_equality() {
    constexpr Pair lhs[2] = {{1, 10}, {2, 20}};
    constexpr Pair rhs[2] = {{1, 70}, {2, 80}};

    nad_Arr *a = nullptr;
    nad_Arr *b = nullptr;
    NAD_TEST_OK(NAD_ARR_FROM_DATA(Pair, lhs, 2, nad_al_default(), &a));
    NAD_TEST_OK(NAD_ARR_FROM_DATA(Pair, rhs, 2, nad_al_default(), &b));

    TEST_ASSERT_FALSE(nad_arr_eq(a, b));
    TEST_ASSERT_TRUE(nad_arr_eq_by(a, b, nad_test_pair_eq_a));

    nad_arr_drop(a);
    nad_arr_drop(b);
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
    RUN_TEST(test_swap_across_allocators_moves_the_bytes);
    RUN_TEST(test_swap_across_allocators_with_an_empty_side);
    RUN_TEST(test_swap_across_allocators_reports_a_failed_first_alloc);
    RUN_TEST(test_swap_across_allocators_rolls_back_a_failed_second_alloc);

    RUN_TEST(test_to_span_matches_the_arr_shape);
    RUN_TEST(test_to_span_of_empty_keeps_elem_size);

    RUN_TEST(test_copy_inherits_the_source_allocator);
    RUN_TEST(test_copy_with_builds_on_the_given_allocator);
    RUN_TEST(test_copy_with_reports_an_exhausted_target_arena);
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

    return UNITY_END();
}
