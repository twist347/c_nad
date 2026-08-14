#include "nad/ds/arr.h"
#include "nad/alloc/alloc_default.h"
#include "nad/alloc/alloc_arena.h"

#include "unity.h"

#include <stdint.h>

#include "nad/core/util.h"

void setUp() {
}

void tearDown() {
}

// an elem wider than a word, to keep elem_size honest
typedef struct {
    int64_t a;
    int64_t b;
} Pair;

// burns arena space until exactly `want` bytes are left
static void arena_leave(nad_Al *arena, size_t want) {
    const size_t available = nad_al_arena_stats(arena).available;
    TEST_ASSERT_TRUE(available >= want);

    const size_t burn = available - want;
    if (burn > 0) {
        TEST_ASSERT_NOT_NULL(nad_alloc(arena, burn));
    }
    TEST_ASSERT_EQUAL_size_t(want, nad_al_arena_stats(arena).available);
}

// int32_t array holding 0, 1, ... len-1
static nad_Arr *make_arr(size_t len) {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_NEW_LEN(int32_t, len, nad_al_default(), &a));

    for (size_t i = 0; i < len; ++i) {
        NAD_ARR_SET(int32_t, a, i, (int32_t) i);
    }
    return a;
}

/* ========== lifetime ========== */

static void test_new_sets_shape_and_zeroes() {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_NEW_LEN(int32_t, 4, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(4, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_arr_al(a));

    constexpr int32_t zeroes[4] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT32_ARRAY(zeroes, nad_arr_data(a), 4);

    nad_arr_drop(a);
}

static void test_new_empty_has_no_buffer() {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_NEW_LEN(int32_t, 0, nad_al_default(), &a));

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

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_swap(a, b));

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
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_from_span(s, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_EQUAL_INT32_ARRAY(src, nad_arr_data(a), 3);
    TEST_ASSERT_TRUE((const void *) src != nad_arr_data(a));

    nad_arr_drop(a);
}

static void test_from_span_empty_has_no_buffer() {
    const nad_Span s = NAD_SPAN_NEW(int32_t, nullptr, 0);

    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_from_span(s, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(0, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));
    TEST_ASSERT_NULL(nad_arr_data(a));

    nad_arr_drop(a);
}

// arr -> span -> arr must round-trip without touching the original
static void test_from_span_of_an_arr_round_trips() {
    nad_Arr *src = make_arr(4);

    nad_Arr *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_from_span(nad_arr_to_span(src), nad_al_default(), &dst));

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
    const Pair src[2] = {{1, 2}, {3, 4}};

    nad_Arr *arr = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_FROM_DATA(Pair, src, 2, nad_al_default(), &arr));

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

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_swap(a, a));

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

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_swap(a, b));

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
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_OF(int32_t, arena, &b, 10, 20, 30));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_swap(a, b));

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
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_OF(int32_t, arena, &b, 1, 2, 3));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_swap(a, b));

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
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_OF(int32_t, arena, &a, 1, 2));

    nad_Arr *b = make_arr(3); // default: 0, 1, 2

    const void *pa = nad_arr_data(a);
    const void *pb = nad_arr_data(b);

    arena_leave(arena, 0);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OUT_OF_MEMORY, nad_arr_swap(a, b));

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
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_OF(int32_t, arena, &b, 10, 20, 30));

    const void *pa = nad_arr_data(a);
    const void *pb = nad_arr_data(b);

    // b's side has nothing left to give
    arena_leave(arena, 0);

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OUT_OF_MEMORY, nad_arr_swap(a, b));

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
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_OF(int32_t, arena, &src, 1, 2, 3));

    nad_Arr *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, nad_arr_al(dst));
    TEST_ASSERT_EQUAL_INT32_ARRAY(nad_arr_data(src), nad_arr_data(dst), 3);

    nad_arr_drop(dst);
    nad_arr_drop(src);
    nad_al_arena_drop(arena);
}

// assignment resizes through the target's allocator, not the source's
static void test_copy_assign_keeps_the_target_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *src = make_arr(4);

    nad_Arr *dst = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_NEW_LEN(int32_t, 1, arena, &dst));

    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, nad_arr_copy_assign(src, dst));

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

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OUT_OF_MEMORY,
        nad_arr_new_len(SIZE_MAX, 2, nad_al_default(), &a)
    );

    TEST_ASSERT_NULL(a); // out is untouched on failure
}

static void test_from_data_reports_size_overflow() {
    constexpr int32_t src[1] = {1};
    nad_Arr *a = nullptr;

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OUT_OF_MEMORY,
        nad_arr_from_data(src, SIZE_MAX, 2, nad_al_default(), &a)
    );

    TEST_ASSERT_NULL(a);
}

static void test_new_len_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Arr *a = nullptr;

    TEST_ASSERT_EQUAL_INT(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_ARR_NEW_LEN(int32_t, 1000, arena, &a)
    );

    TEST_ASSERT_NULL(a);

    nad_al_arena_drop(arena);
}

/* ========== macros ========== */

static void test_macro_of_builds_from_literals() {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_OF(int32_t, nad_al_default(), &a, 4, 5, 6));

    TEST_ASSERT_EQUAL_size_t(3, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_arr_elem_size(a));

    constexpr int32_t want[3] = {4, 5, 6};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, nad_arr_data(a), 3);

    nad_arr_drop(a);
}

static void test_macro_of_derives_len_from_the_list() {
    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_OF(int64_t, nad_al_default(), &a, 1, 2, 3, 4, 5));

    TEST_ASSERT_EQUAL_size_t(5, nad_arr_len(a));
    TEST_ASSERT_EQUAL_size_t(sizeof(int64_t), nad_arr_elem_size(a));

    nad_arr_drop(a);
}

static void test_macro_from_data_infers_elem_size() {
    constexpr int32_t src[2] = {1, 2};

    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_FROM_DATA(int32_t, src, 2, nad_al_default(), &a));

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
    const Pair src[2] = {{1, 2}, {3, 4}};

    nad_Arr *a = nullptr;
    TEST_ASSERT_EQUAL_INT(NAD_STATUS_OK, NAD_ARR_FROM_DATA(Pair, src, 2, nad_al_default(), &a));

    TEST_ASSERT_EQUAL_size_t(2 * sizeof(Pair), nad_arr_bytes(a));

    nad_arr_drop(a);
}

// the arr and its view must agree on the size of the same memory
static void test_bytes_agrees_with_the_span() {
    nad_Arr *a = make_arr(3);

    TEST_ASSERT_EQUAL_size_t(nad_span_bytes(nad_arr_to_span(a)), nad_arr_bytes(a));

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
    RUN_TEST(test_copy_assign_keeps_the_target_allocator);

    RUN_TEST(test_new_len_reports_size_overflow);
    RUN_TEST(test_from_data_reports_size_overflow);
    RUN_TEST(test_new_len_reports_an_exhausted_arena);

    RUN_TEST(test_macro_of_builds_from_literals);
    RUN_TEST(test_macro_of_derives_len_from_the_list);
    RUN_TEST(test_macro_from_data_infers_elem_size);

    RUN_TEST(test_bytes_is_len_times_elem_size);
    RUN_TEST(test_bytes_of_empty_is_zero);
    RUN_TEST(test_bytes_tracks_elem_size);
    RUN_TEST(test_bytes_agrees_with_the_span);

    return UNITY_END();
}
