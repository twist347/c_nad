#include "trs/ds/pqueue.h"
#include "trs/algo/heap.h"
#include "trs/algo/permute.h"
#include "trs/algo/sort.h"
#include "trs/alloc/arena.h"
#include "trs/alloc/default.h"
#include "trs/core/print.h"
#include "trs/ds/vec.h"

#include "support/arena.h"
#include "support/pair.h"
#include "support/probe.h"
#include "support/status.h"

#include <unity.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp() {
}

void tearDown() {
}

/* ========== helpers ========== */

// order over Pair by its first field, written the way cmp.h prescribes: a comparator for
// a struct delegates to the one for the field it orders by
static int cmp_pair_a(const void *lhs, const void *rhs) {
    return trs_cmp_i64(&((const Pair *) lhs)->a, &((const Pair *) rhs)->a);
}

static void push_int(trs_PQueue *q, int32_t val) {
    TRS_TEST_OK(trs_pqueue_push(q, &val));
}

// max-queue over the default allocator, filled one push at a time
static trs_PQueue *make_queue(const int32_t *src, size_t n) {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_NEW(int32_t, trs_cmp_i32, trs_al_default(), &q));

    for (size_t i = 0; i < n; ++i) {
        push_int(q, src[i]);
    }
    return q;
}

// the same elems, heapified in one go instead
static trs_PQueue *make_queue_from(const int32_t *src, size_t n) {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_FROM_DATA(int32_t, src, n, trs_cmp_i32, trs_al_default(), &q));

    return q;
}

// what the queue must hand back: a copy sorted greatest first by the libc qsort, an
// oracle that shares no code with what is tested. trs_Cmp is qsort compatible by design
static void expected_order(int32_t *dst, const int32_t *src, size_t n, trs_Cmp cmp) {
    memcpy(dst, src, n * sizeof(int32_t));
    qsort(dst, n, sizeof(int32_t), cmp);
}

// empties the queue through top + pop, checking at every step that what is left is still
// a heap and one elem shorter. The invariant is checked here rather than in a test of its
// own because it must hold after EVERY pop, not just the last one
static void assert_drains(trs_PQueue *q, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_pqueue_len(q));

    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_INT32(want[i], *TRS_PQUEUE_TOP_AS(int32_t, q));
        trs_pqueue_pop(q);

        TEST_ASSERT_EQUAL_size_t(n - i - 1, trs_pqueue_len(q));
        TEST_ASSERT_TRUE(trs_span_is_heap(trs_pqueue_to_span(q), trs_pqueue_cmp(q)));
    }
}

static void assert_drains_sorted(trs_PQueue *q, const int32_t *src, size_t n, trs_Cmp cmp) {
    int32_t want[64];
    TEST_ASSERT_TRUE(n <= 64);

    expected_order(want, src, n, cmp);
    assert_drains(q, want, n);
}

// walks every permutation of {1..n} and hands each to 'check'. next_permutation is a data
// generator here, not an oracle: were it broken the sweep would visit fewer inputs, which
// cannot turn a failing case into a passing one
static void for_every_permutation(size_t n, void (*check)(const int32_t *, size_t)) {
    int32_t buf[8];
    TEST_ASSERT_TRUE(n <= 8);

    for (size_t i = 0; i < n; ++i) {
        buf[i] = (int32_t) i + 1;
    }

    size_t seen = 0;
    do {
        check(buf, n);
        ++seen;
    } while (trs_span_next_permutation(TRS_SPAN_FROM_DATA_MUT(int32_t, buf, n), trs_cmp_i32));

    size_t want = 1;
    for (size_t i = 2; i <= n; ++i) {
        want *= i;
    }
    TEST_ASSERT_EQUAL_size_t(want, seen);
}

// a spread out input with duplicates: nothing here is sorted, reversed or unique
static constexpr int32_t SPREAD[] = {5, 1, 9, 9, 3, 7, 2, 8, 3, 6, 0, 4, 9, 1};
static constexpr size_t SPREAD_LEN = sizeof(SPREAD) / sizeof(SPREAD[0]);

/* ========== lifetime ========== */

static void test_new_starts_empty_and_keeps_the_comparator() {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_NEW(int32_t, trs_cmp_i32, trs_al_default(), &q));

    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(q));
    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_cap(q));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_pqueue_elem_size(q));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_pqueue_al(q));
    TEST_ASSERT_EQUAL_PTR(trs_cmp_i32, trs_pqueue_cmp(q));

    trs_pqueue_drop(q);
}

// cap is room, not content
static void test_new_cap_reserves_without_length() {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_NEW_CAP(int32_t, 8, trs_cmp_i32, trs_al_default(), &q));

    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(q));
    TEST_ASSERT_EQUAL_size_t(8, trs_pqueue_cap(q));

    trs_pqueue_drop(q);
}

static void test_from_data_heapifies_what_it_is_given() {
    trs_PQueue *q = make_queue_from(SPREAD, SPREAD_LEN);

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_pqueue_len(q));
    TEST_ASSERT_TRUE(trs_span_is_heap(trs_pqueue_to_span(q), trs_cmp_i32));

    trs_pqueue_drop(q);
}

static void test_from_data_empty_stays_empty() {
    trs_PQueue *q = make_queue_from(nullptr, 0);

    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(q));
    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_cap(q));

    trs_pqueue_drop(q);
}

// the queue owns its elems: writing over the source afterwards must not reach them
static void test_from_span_copies_the_view() {
    int32_t src[] = {3, 1, 2};
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(trs_pqueue_from_span(TRS_SPAN_FROM_DATA(int32_t, src, 3), trs_cmp_i32, trs_al_default(), &q));

    src[0] = 100;
    src[1] = 200;
    src[2] = 300;

    constexpr int32_t want[] = {3, 2, 1};
    assert_drains(q, want, 3);

    trs_pqueue_drop(q);
}

// an elem wider than a word, to catch a copy that moves bytes by the wrong stride
static void test_from_data_copies_whole_elems() {
    constexpr Pair src[] = {{.a = 1, .b = 10}, {.a = 3, .b = 30}, {.a = 2, .b = 20}};
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_FROM_DATA(Pair, src, 3, cmp_pair_a, trs_al_default(), &q));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), trs_pqueue_elem_size(q));

    const Pair *top = TRS_PQUEUE_TOP_AS(Pair, q);
    TEST_ASSERT_EQUAL_INT64(3, top->a);
    TEST_ASSERT_EQUAL_INT64(30, top->b);

    trs_pqueue_drop(q);
}

static void test_drop_null_is_noop() {
    trs_pqueue_drop(nullptr);
}

/* ========== order ========== */

static void test_pushes_drain_greatest_first() {
    trs_PQueue *q = make_queue(SPREAD, SPREAD_LEN);

    assert_drains_sorted(q, SPREAD, SPREAD_LEN, trs_cmp_desc_i32);

    trs_pqueue_drop(q);
}

// every length up to the spread, so growing across a reallocation is covered too
static void test_pushes_drain_greatest_first_at_every_length() {
    for (size_t n = 0; n <= SPREAD_LEN; ++n) {
        trs_PQueue *q = make_queue(SPREAD, n);
        assert_drains_sorted(q, SPREAD, n, trs_cmp_desc_i32);
        trs_pqueue_drop(q);
    }
}

static void check_drains_in_order(const int32_t *src, size_t n) {
    trs_PQueue *pushed = make_queue(src, n);
    assert_drains_sorted(pushed, src, n, trs_cmp_desc_i32);
    trs_pqueue_drop(pushed);

    // the two ways in must agree: heapifying is an optimisation, not another order
    trs_PQueue *heapified = make_queue_from(src, n);
    assert_drains_sorted(heapified, src, n, trs_cmp_desc_i32);
    trs_pqueue_drop(heapified);
}

// every arrangement of five distinct elems, all 120 of them
static void test_every_permutation_drains_in_order() {
    for_every_permutation(5, check_drains_in_order);
}

// there is no min-queue type: a descending comparator is the whole difference
static void test_a_descending_comparator_gives_a_min_queue() {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_desc_i32, trs_al_default(), &q, 5, 3, 9, 1, 7));

    assert_drains_sorted(q, (const int32_t[]){5, 3, 9, 1, 7}, 5, trs_cmp_i32);

    trs_pqueue_drop(q);
}

static void test_top_is_the_greatest_after_every_push() {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_NEW(int32_t, trs_cmp_i32, trs_al_default(), &q));

    int32_t high = SPREAD[0];
    for (size_t i = 0; i < SPREAD_LEN; ++i) {
        push_int(q, SPREAD[i]);
        if (SPREAD[i] > high) {
            high = SPREAD[i];
        }
        TEST_ASSERT_EQUAL_INT32(high, *TRS_PQUEUE_TOP_AS(int32_t, q));
        TEST_ASSERT_TRUE(trs_span_is_heap(trs_pqueue_to_span(q), trs_cmp_i32));
    }

    trs_pqueue_drop(q);
}

// popping to empty and refilling: the queue must not be a one-shot
static void test_pushes_after_a_full_drain_are_ordered_again() {
    trs_PQueue *q = make_queue(SPREAD, SPREAD_LEN);

    for (size_t i = 0; i < SPREAD_LEN; ++i) {
        trs_pqueue_pop(q);
    }
    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(q));

    constexpr int32_t again[] = {2, 8, 5};
    for (size_t i = 0; i < 3; ++i) {
        push_int(q, again[i]);
    }
    assert_drains_sorted(q, again, 3, trs_cmp_desc_i32);

    trs_pqueue_drop(q);
}

// SPREAD holds 9 three times and 1 and 3 twice: a duplicate is an elem, not a set member
static void test_duplicates_all_come_back() {
    trs_PQueue *q = make_queue_from(SPREAD, SPREAD_LEN);

    size_t nines = 0;
    while (trs_pqueue_len(q) > 0 && *TRS_PQUEUE_TOP_AS(int32_t, q) == 9) {
        ++nines;
        trs_pqueue_pop(q);
    }
    TEST_ASSERT_EQUAL_size_t(3, nines);
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN - 3, trs_pqueue_len(q));

    trs_pqueue_drop(q);
}

// On equal keys nothing is promised about the order, but every payload must survive
// exactly once. Distinct int32_t elems cannot witness this: with equal keys the elems are
// indistinguishable, so a shuffle, a loss and a duplicate all look the same
static void test_equal_keys_keep_every_payload() {
    constexpr Pair src[] = {
        {.a = 7, .b = 1}, {.a = 7, .b = 2}, {.a = 7, .b = 3},
        {.a = 7, .b = 4}, {.a = 7, .b = 5}, {.a = 7, .b = 6},
    };
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_FROM_DATA(Pair, src, 6, cmp_pair_a, trs_al_default(), &q));

    bool seen[7] = {};
    for (size_t i = 0; i < 6; ++i) {
        const Pair *top = TRS_PQUEUE_TOP_AS(Pair, q);
        TEST_ASSERT_EQUAL_INT64(7, top->a);
        TEST_ASSERT_TRUE(top->b >= 1 && top->b <= 6);
        TEST_ASSERT_FALSE_MESSAGE(seen[top->b], "a payload came back twice");
        seen[top->b] = true;
        trs_pqueue_pop(q);
    }

    trs_pqueue_drop(q);
}

/* ========== access ========== */

static void test_top_reads_without_removing() {
    trs_PQueue *q = make_queue(SPREAD, SPREAD_LEN);

    TEST_ASSERT_EQUAL_INT32(9, *TRS_PQUEUE_TOP_AS(int32_t, q));
    TEST_ASSERT_EQUAL_INT32(9, *TRS_PQUEUE_TOP_AS(int32_t, q));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_pqueue_len(q));

    trs_pqueue_drop(q);
}

/* ========== info ========== */

static void test_len_follows_push_and_pop() {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_NEW(int32_t, trs_cmp_i32, trs_al_default(), &q));

    for (size_t i = 0; i < 5; ++i) {
        push_int(q, (int32_t) i);
        TEST_ASSERT_EQUAL_size_t(i + 1, trs_pqueue_len(q));
    }
    for (size_t i = 5; i > 0; --i) {
        TEST_ASSERT_EQUAL_size_t(i, trs_pqueue_len(q));
        trs_pqueue_pop(q);
    }
    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(q));

    trs_pqueue_drop(q);
}

// a pop frees no memory, exactly as it does not in the vec underneath
static void test_pop_leaves_the_capacity_alone() {
    trs_PQueue *q = make_queue(SPREAD, SPREAD_LEN);
    const size_t cap = trs_pqueue_cap(q);

    trs_pqueue_pop(q);

    TEST_ASSERT_EQUAL_size_t(cap, trs_pqueue_cap(q));

    trs_pqueue_drop(q);
}

/* ========== mods ========== */

static void test_clear_empties_without_giving_back_the_room() {
    trs_PQueue *q = make_queue(SPREAD, SPREAD_LEN);
    const size_t cap = trs_pqueue_cap(q);

    trs_pqueue_clear(q);

    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(q));
    TEST_ASSERT_EQUAL_size_t(cap, trs_pqueue_cap(q));

    push_int(q, 42);
    TEST_ASSERT_EQUAL_INT32(42, *TRS_PQUEUE_TOP_AS(int32_t, q));

    trs_pqueue_drop(q);
}

static void test_reserve_grows_the_room_only() {
    trs_PQueue *q = make_queue(SPREAD, 3);

    TRS_TEST_OK(trs_pqueue_reserve(q, 100));

    TEST_ASSERT_TRUE(trs_pqueue_cap(q) >= 100);
    TEST_ASSERT_EQUAL_size_t(3, trs_pqueue_len(q));
    TEST_ASSERT_TRUE(trs_span_is_heap(trs_pqueue_to_span(q), trs_cmp_i32));

    trs_pqueue_drop(q);
}

// the buffer moves, and the heap must survive the move
static void test_shrink_to_fit_keeps_the_order() {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_NEW_CAP(int32_t, 64, trs_cmp_i32, trs_al_default(), &q));
    for (size_t i = 0; i < SPREAD_LEN; ++i) {
        push_int(q, SPREAD[i]);
    }

    TRS_TEST_OK(trs_pqueue_shrink_to_fit(q));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_pqueue_cap(q));
    assert_drains_sorted(q, SPREAD, SPREAD_LEN, trs_cmp_desc_i32);

    trs_pqueue_drop(q);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    trs_PQueue *src = make_queue(SPREAD, SPREAD_LEN);
    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(trs_pqueue_copy(src, &dst));

    push_int(dst, 1000);
    trs_pqueue_pop(src);

    TEST_ASSERT_EQUAL_INT32(1000, *TRS_PQUEUE_TOP_AS(int32_t, dst));
    TEST_ASSERT_EQUAL_INT32(9, *TRS_PQUEUE_TOP_AS(int32_t, src));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN + 1, trs_pqueue_len(dst));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN - 1, trs_pqueue_len(src));

    trs_pqueue_drop(src);
    trs_pqueue_drop(dst);
}

static void test_copy_inherits_the_allocator_and_the_comparator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_PQueue *src = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_desc_i32, arena, &src, 5, 1, 3));

    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(trs_pqueue_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_pqueue_al(dst));
    TEST_ASSERT_EQUAL_PTR(trs_cmp_desc_i32, trs_pqueue_cmp(dst));
    TEST_ASSERT_EQUAL_INT32(1, *TRS_PQUEUE_TOP_AS(int32_t, dst));

    trs_pqueue_drop(src);
    trs_pqueue_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_copy_with_builds_on_the_given_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_PQueue *src = make_queue_from(SPREAD, SPREAD_LEN);

    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(trs_pqueue_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_pqueue_al(dst));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_pqueue_al(src));
    TEST_ASSERT_EQUAL_PTR(trs_cmp_i32, trs_pqueue_cmp(dst));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_pqueue_len(dst));

    // the source is gone and the copy still drains in order: the buffer is its own
    trs_pqueue_drop(src);
    assert_drains_sorted(dst, SPREAD, SPREAD_LEN, trs_cmp_desc_i32);

    trs_pqueue_drop(dst);
    trs_al_arena_drop(arena);
}

// the blocks are asked of the allocator the copy is going to, not of the source's
static void test_copy_with_reports_an_exhausted_target_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_PQueue *src = make_queue_from(SPREAD, SPREAD_LEN);

    trs_PQueue *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_pqueue_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_pqueue_len(src));

    trs_pqueue_drop(src);
    trs_al_arena_drop(arena);
}

static void test_move_assign_hands_over_the_contents_on_one_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_PQueue *src = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_i32, &al, &src, 1, 5, 3));

    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_desc_i32, &al, &dst, 9));

    const size_t requests = trs_test_probe_requests(&probe);
    TRS_TEST_OK(trs_pqueue_move_assign(src, dst));

    // nothing was asked of the allocator: the vec's block changed hands
    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    TEST_ASSERT_EQUAL_size_t(3, trs_pqueue_len(dst));
    TEST_ASSERT_EQUAL_INT32(5, *TRS_PQUEUE_TOP_AS(int32_t, dst));

    // the elems arrive arranged under the source's comparator, so it travels with them
    TEST_ASSERT_EQUAL_PTR(trs_cmp_i32, trs_pqueue_cmp(dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(src));

    trs_pqueue_drop(src);
    trs_pqueue_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_move_assign_across_allocators_empties_the_source() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_PQueue *src = make_queue_from(SPREAD, SPREAD_LEN);

    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_desc_i32, arena, &dst, 9));

    TRS_TEST_OK(trs_pqueue_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_pqueue_len(dst));
    TEST_ASSERT_EQUAL_PTR(trs_cmp_i32, trs_pqueue_cmp(dst));
    TEST_ASSERT_EQUAL_PTR(arena, trs_pqueue_al(dst));
    assert_drains_sorted(dst, SPREAD, SPREAD_LEN, trs_cmp_desc_i32);

    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(src));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_pqueue_al(src));

    trs_pqueue_drop(src);
    trs_pqueue_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_across_allocators_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_desc_i32, arena, &dst, 9));
    trs_test_arena_leave(arena, 0);

    trs_PQueue *src = make_queue_from(SPREAD, SPREAD_LEN);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_pqueue_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_pqueue_len(src));
    TEST_ASSERT_EQUAL_size_t(1, trs_pqueue_len(dst));
    TEST_ASSERT_EQUAL_PTR(trs_cmp_desc_i32, trs_pqueue_cmp(dst));

    trs_pqueue_drop(src);
    trs_pqueue_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_of_itself_changes_nothing() {
    trs_PQueue *q = make_queue_from(SPREAD, SPREAD_LEN);

    TRS_TEST_OK(trs_pqueue_move_assign(q, q));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_pqueue_len(q));
    assert_drains_sorted(q, SPREAD, SPREAD_LEN, trs_cmp_desc_i32);

    trs_pqueue_drop(q);
}

static void test_copy_drains_the_same_as_its_source() {
    trs_PQueue *src = make_queue_from(SPREAD, SPREAD_LEN);
    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(trs_pqueue_copy(src, &dst));

    assert_drains_sorted(dst, SPREAD, SPREAD_LEN, trs_cmp_desc_i32);

    trs_pqueue_drop(src);
    trs_pqueue_drop(dst);
}

static void test_copy_of_empty_stays_empty() {
    trs_PQueue *src = make_queue(nullptr, 0);
    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(trs_pqueue_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_len(dst));

    trs_pqueue_drop(src);
    trs_pqueue_drop(dst);
}

// the comparator is part of the value: a heap means nothing without the order it was
// built under, so the target's own comparator is replaced rather than kept
static void test_copy_assign_hands_over_the_comparator_too() {
    trs_PQueue *src = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_desc_i32, trs_al_default(), &src, 5, 1, 3));

    trs_PQueue *dst = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_i32, trs_al_default(), &dst, 100, 200, 300));

    TRS_TEST_OK(trs_pqueue_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_PTR(trs_cmp_desc_i32, trs_pqueue_cmp(dst));
    TEST_ASSERT_TRUE(trs_span_is_heap(trs_pqueue_to_span(dst), trs_pqueue_cmp(dst)));
    assert_drains_sorted(dst, (const int32_t[]){5, 1, 3}, 3, trs_cmp_i32);

    trs_pqueue_drop(src);
    trs_pqueue_drop(dst);
}

static void test_copy_assign_self_is_noop() {
    trs_PQueue *q = make_queue(SPREAD, SPREAD_LEN);

    TRS_TEST_OK(trs_pqueue_copy_assign(q, q));

    assert_drains_sorted(q, SPREAD, SPREAD_LEN, trs_cmp_desc_i32);

    trs_pqueue_drop(q);
}

/* ========== swap ========== */

static void test_swap_exchanges_the_elems_and_the_comparators() {
    trs_PQueue *a = nullptr;
    trs_PQueue *b = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_i32, trs_al_default(), &a, 1, 2, 3));
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_desc_i32, trs_al_default(), &b, 10, 20, 30));

    trs_pqueue_swap(a, b);

    TEST_ASSERT_EQUAL_PTR(trs_cmp_desc_i32, trs_pqueue_cmp(a));
    TEST_ASSERT_EQUAL_PTR(trs_cmp_i32, trs_pqueue_cmp(b));
    TEST_ASSERT_EQUAL_INT32(10, *TRS_PQUEUE_TOP_AS(int32_t, a));
    TEST_ASSERT_EQUAL_INT32(3, *TRS_PQUEUE_TOP_AS(int32_t, b));

    // each side is still a heap under the order that arrived with the elems
    TEST_ASSERT_TRUE(trs_span_is_heap(trs_pqueue_to_span(a), trs_pqueue_cmp(a)));
    TEST_ASSERT_TRUE(trs_span_is_heap(trs_pqueue_to_span(b), trs_pqueue_cmp(b)));

    trs_pqueue_drop(a);
    trs_pqueue_drop(b);
}

static void test_swap_self_is_noop() {
    trs_PQueue *q = make_queue(SPREAD, SPREAD_LEN);

    trs_pqueue_swap(q, q);

    assert_drains_sorted(q, SPREAD, SPREAD_LEN, trs_cmp_desc_i32);

    trs_pqueue_drop(q);
}

/* ========== to span ========== */

// heap order is weaker than sorted order: only the first elem is where it will end up.
// The span is the buffer as it stands, not a sorted run and not a copy
static void test_to_span_shows_the_heap_not_a_sorted_run() {
    trs_PQueue *q = make_queue_from(SPREAD, SPREAD_LEN);
    const trs_Span s = trs_pqueue_to_span(q);

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, s.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), s.elem_size);
    TEST_ASSERT_EQUAL_PTR(trs_pqueue_top(q), s.data);
    TEST_ASSERT_TRUE(trs_span_is_heap(s, trs_cmp_i32));
    TEST_ASSERT_FALSE(trs_span_is_sorted(s, trs_cmp_desc_i32));

    trs_pqueue_drop(q);
}

static void test_to_span_of_empty_has_no_elems() {
    trs_PQueue *q = make_queue(nullptr, 0);

    TEST_ASSERT_EQUAL_size_t(0, trs_pqueue_to_span(q).len);

    trs_pqueue_drop(q);
}

/* ========== failure ========== */

static void test_new_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_PQueue *q = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_PQUEUE_NEW(int32_t, trs_cmp_i32, arena, &q));
    TEST_ASSERT_NULL(q);

    trs_al_arena_drop(arena);
}

static void test_from_data_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 128);
    TEST_ASSERT_NOT_NULL(arena);

    trs_PQueue *q = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_PQUEUE_FROM_DATA(int32_t, SPREAD, 1000, trs_cmp_i32, arena, &q));
    TEST_ASSERT_NULL(q);

    trs_al_arena_drop(arena);
}

// a refused push must leave the queue exactly as it was — not a heap with a hole in it
static void test_push_reports_an_exhausted_arena_and_changes_nothing() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_i32, arena, &q, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    constexpr int32_t val = 100;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_pqueue_push(q, &val));

    TEST_ASSERT_EQUAL_size_t(3, trs_pqueue_len(q));
    TEST_ASSERT_EQUAL_INT32(5, *TRS_PQUEUE_TOP_AS(int32_t, q));
    TEST_ASSERT_TRUE(trs_span_is_heap(trs_pqueue_to_span(q), trs_cmp_i32));

    trs_pqueue_drop(q);
    trs_al_arena_drop(arena);
}

static void test_copy_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_PQueue *src = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_i32, arena, &src, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    trs_PQueue *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_pqueue_copy(src, &dst));
    TEST_ASSERT_NULL(dst);

    trs_pqueue_drop(src);
    trs_al_arena_drop(arena);
}

static void test_reserve_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_i32, arena, &q, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_pqueue_reserve(q, 1000));
    TEST_ASSERT_EQUAL_size_t(3, trs_pqueue_len(q));

    trs_pqueue_drop(q);
    trs_al_arena_drop(arena);
}

// The queue is built in two allocations, the buffer and then the header. When the second
// one is refused the first must not be stranded: the probe counts what is still live, and
// an arena would hide the leak because it frees everything at once
static void test_a_refused_header_frees_the_buffer() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 1);

    trs_PQueue *q = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_PQUEUE_NEW(int32_t, trs_cmp_i32, &al, &q));

    TEST_ASSERT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the same, one allocation later: from_data takes the buffer as well
static void test_a_refused_header_frees_a_filled_buffer() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 2);

    trs_PQueue *q = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_PQUEUE_FROM_DATA(int32_t, SPREAD, SPREAD_LEN, trs_cmp_i32, &al, &q));

    TEST_ASSERT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== into ========== */

// the vec was there all along: taking it copies nothing and hands the elems over in heap
// order, which is not sorted order
static void test_into_vec_hands_the_elems_over() {
    constexpr int32_t src[6] = {3, 1, 4, 1, 5, 9};
    trs_PQueue *q = make_queue_from(src, 6);

    const void *before = trs_pqueue_to_span(q).data;
    const size_t cap = trs_pqueue_cap(q);

    trs_Vec *v = trs_pqueue_into_vec(q);

    TEST_ASSERT_EQUAL_PTR(before, trs_vec_data(v));
    TEST_ASSERT_EQUAL_size_t(6, trs_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(cap, trs_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_vec_al(v));

    // still a heap, so the greatest is at the front and the rest are not sorted yet
    TEST_ASSERT_TRUE(trs_span_is_heap(trs_vec_to_span(v), trs_cmp_i32));
    TEST_ASSERT_EQUAL_INT32(9, *TRS_VEC_GET_AS(int32_t, v, 0));

    trs_vec_drop(v);
}

// the whole point of handing the vec over: sort_heap finishes the job in place, which is
// heapsort's second half
static void test_into_vec_then_sort_heap() {
    constexpr int32_t src[6] = {3, 1, 4, 1, 5, 9};
    constexpr int32_t want[6] = {1, 1, 3, 4, 5, 9};

    trs_Vec *v = trs_pqueue_into_vec(make_queue_from(src, 6));

    trs_span_sort_heap(trs_vec_to_span_mut(v), trs_cmp_i32);

    for (size_t i = 0; i < 6; ++i) {
        TEST_ASSERT_EQUAL_INT32(want[i], *TRS_VEC_GET_AS(int32_t, v, i));
    }

    trs_vec_drop(v);
}

static void test_into_vec_of_an_empty_queue() {
    constexpr int32_t src[1] = {7};
    trs_Vec *v = trs_pqueue_into_vec(make_queue(src, 0));

    TEST_ASSERT_EQUAL_size_t(0, trs_vec_len(v));

    trs_vec_drop(v);
}

// only the adapter's own header goes back; the comparator does not travel with the elems
static void test_into_vec_releases_the_header_alone() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_NEW(int32_t, trs_cmp_i32, &al, &q));

    // the last block the constructor took is the adapter's own header, so this is what
    // into has to hand back, and with the size it was taken as
    const size_t header = probe.last_alloc_size;

    TRS_TEST_OK(TRS_PQUEUE_PUSH(int32_t, q, 1));
    const size_t live = probe.live;

    trs_Vec *v = trs_pqueue_into_vec(q);

    TEST_ASSERT_EQUAL_size_t(live - 1, probe.live);
    TEST_ASSERT_EQUAL_size_t(header, probe.last_dealloc_size);

    trs_vec_drop(v);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== print ========== */

// a printer writes to a stream, so a case has to read one back. tmpfile is the portable
// way, the same one test/core/test_print.c takes
static void assert_prints(const char *expected, const trs_PQueue *q) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);

    trs_pqueue_fprint(q, stream, trs_fprint_i32);
    rewind(stream);

    char buf[128];
    const size_t n = fread(buf, 1, sizeof buf - 1, stream);
    buf[n] = '\0';
    fclose(stream);

    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_fprint_writes_the_elem() {
    trs_PQueue *q = nullptr;
    TRS_TEST_OK(TRS_PQUEUE_OF(int32_t, trs_cmp_i32, trs_al_default(), &q, 7));

    assert_prints("[7]\n", q);

    trs_pqueue_drop(q);
}

// with more than one elem the order is the heap's, which the header calls unspecified —
// so a case may say what is printed only where there is nothing to order
static void test_fprint_of_an_empty_pqueue() {
    trs_PQueue *q = make_queue(nullptr, 0);

    assert_prints("[]\n", q);

    trs_pqueue_drop(q);
}

// the stdout twin takes no stream, and C has no portable way to capture one and give it
// back — so a case can only say that it runs and reaches the same printer
static void test_print_writes_to_stdout() {
    constexpr int32_t src[3] = {1, 2, 3};
    trs_PQueue *q = make_queue(src, 3);

    trs_pqueue_print(q, trs_fprint_i32);

    trs_pqueue_drop(q);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_empty_and_keeps_the_comparator);
    RUN_TEST(test_new_cap_reserves_without_length);
    RUN_TEST(test_from_data_heapifies_what_it_is_given);
    RUN_TEST(test_from_data_empty_stays_empty);
    RUN_TEST(test_from_span_copies_the_view);
    RUN_TEST(test_from_data_copies_whole_elems);
    RUN_TEST(test_drop_null_is_noop);

    RUN_TEST(test_pushes_drain_greatest_first);
    RUN_TEST(test_pushes_drain_greatest_first_at_every_length);
    RUN_TEST(test_every_permutation_drains_in_order);
    RUN_TEST(test_a_descending_comparator_gives_a_min_queue);
    RUN_TEST(test_top_is_the_greatest_after_every_push);
    RUN_TEST(test_pushes_after_a_full_drain_are_ordered_again);
    RUN_TEST(test_duplicates_all_come_back);
    RUN_TEST(test_equal_keys_keep_every_payload);

    RUN_TEST(test_top_reads_without_removing);

    RUN_TEST(test_len_follows_push_and_pop);
    RUN_TEST(test_pop_leaves_the_capacity_alone);

    RUN_TEST(test_clear_empties_without_giving_back_the_room);
    RUN_TEST(test_reserve_grows_the_room_only);
    RUN_TEST(test_shrink_to_fit_keeps_the_order);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_inherits_the_allocator_and_the_comparator);
    RUN_TEST(test_copy_with_builds_on_the_given_allocator);
    RUN_TEST(test_copy_with_reports_an_exhausted_target_arena);
    RUN_TEST(test_move_assign_hands_over_the_contents_on_one_allocator);
    RUN_TEST(test_move_assign_across_allocators_empties_the_source);
    RUN_TEST(test_move_assign_across_allocators_reports_an_exhausted_arena);
    RUN_TEST(test_move_assign_of_itself_changes_nothing);
    RUN_TEST(test_copy_drains_the_same_as_its_source);
    RUN_TEST(test_copy_of_empty_stays_empty);
    RUN_TEST(test_copy_assign_hands_over_the_comparator_too);
    RUN_TEST(test_copy_assign_self_is_noop);

    RUN_TEST(test_swap_exchanges_the_elems_and_the_comparators);
    RUN_TEST(test_swap_self_is_noop);

    RUN_TEST(test_to_span_shows_the_heap_not_a_sorted_run);
    RUN_TEST(test_to_span_of_empty_has_no_elems);

    RUN_TEST(test_new_reports_an_exhausted_arena);
    RUN_TEST(test_from_data_reports_an_exhausted_arena);
    RUN_TEST(test_push_reports_an_exhausted_arena_and_changes_nothing);
    RUN_TEST(test_copy_reports_an_exhausted_arena);
    RUN_TEST(test_reserve_reports_an_exhausted_arena);
    RUN_TEST(test_a_refused_header_frees_the_buffer);
    RUN_TEST(test_a_refused_header_frees_a_filled_buffer);


    RUN_TEST(test_into_vec_hands_the_elems_over);
    RUN_TEST(test_into_vec_then_sort_heap);
    RUN_TEST(test_into_vec_of_an_empty_queue);
    RUN_TEST(test_into_vec_releases_the_header_alone);

    RUN_TEST(test_fprint_writes_the_elem);
    RUN_TEST(test_fprint_of_an_empty_pqueue);
    RUN_TEST(test_print_writes_to_stdout);

    return UNITY_END();
}
