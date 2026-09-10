#include "trs/ds/queue.h"
#include "trs/algo/search.h"
#include "trs/algo/sort.h"
#include "trs/alloc/arena.h"
#include "trs/alloc/default.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"

#include "support/arena.h"
#include "support/pair.h"
#include "support/probe.h"
#include "support/status.h"

#include <unity.h>

#include <stddef.h>
#include <stdint.h>

void setUp() {
}

void tearDown() {
}

/* ========== helpers ========== */

static constexpr int32_t SPREAD[] = {5, 1, 9, 9, 3, 7, 2, 8, 3, 6, 0, 4, 9, 1};
static constexpr size_t SPREAD_LEN = sizeof(SPREAD) / sizeof(SPREAD[0]);

static void push_int(trs_Queue *q, int32_t val) {
    TRS_TEST_OK(trs_queue_push(q, &val));
}

// int32_t queue over the default allocator, filled one push at a time
[[nodiscard]]
static trs_Queue *make_queue(const int32_t *src, size_t n) {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW(int32_t, trs_al_default(), &q));

    for (size_t i = 0; i < n; ++i) {
        push_int(q, src[i]);
    }
    return q;
}

// the same elems handed over in one go instead
[[nodiscard]]
static trs_Queue *make_queue_from(const int32_t *src, size_t n) {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_FROM_DATA(int32_t, src, n, trs_al_default(), &q));

    return q;
}

// {10, 20, 30, 40} in a ring of exactly four slots that is split across the end of the
// buffer. The queue hides the ring, so the proof is the capacity: two pops freed the two
// leading slots and the two pushes that follow fit without growing, which they could only
// do by wrapping around
[[nodiscard]]
static trs_Queue *make_wrapped(void) {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW_CAP(int32_t, 4, trs_al_default(), &q));

    push_int(q, 1);
    push_int(q, 2);
    push_int(q, 10);
    push_int(q, 20);

    trs_queue_pop(q);
    trs_queue_pop(q);

    push_int(q, 30);
    push_int(q, 40);

    TEST_ASSERT_EQUAL_size_t(4, trs_queue_cap(q));
    TEST_ASSERT_EQUAL_size_t(4, trs_queue_len(q));

    return q;
}

// reads the contents without disturbing them — the only way in, since the queue hands
// out no index
static void assert_elems(const trs_Queue *q, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_queue_len(q));

    int32_t got[64];
    TEST_ASSERT_TRUE(n <= 64);

    trs_queue_copy_to_span(q, TRS_SPAN_FROM_DATA_MUT(int32_t, got, n));
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, got, n);
}

// empties the queue through front + pop, checking at every step that the oldest elem is
// the one on offer and that exactly one left. Draining is checked here rather than in a
// test of its own because the order must hold after EVERY pop, not just the last one
static void assert_drains(trs_Queue *q, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_queue_len(q));

    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_INT32(want[i], *TRS_QUEUE_FRONT_AS(int32_t, q));
        TEST_ASSERT_EQUAL_INT32(want[n - 1], *TRS_QUEUE_BACK_AS(int32_t, q));
        trs_queue_pop(q);

        TEST_ASSERT_EQUAL_size_t(n - i - 1, trs_queue_len(q));
    }
}

/* ========== lifetime ========== */

static void test_new_starts_empty() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW(int32_t, trs_al_default(), &q));

    TEST_ASSERT_EQUAL_size_t(0, trs_queue_len(q));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_queue_elem_size(q));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_queue_al(q));

    trs_queue_drop(q);
}

static void test_new_cap_reserves_without_length() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW_CAP(int32_t, 16, trs_al_default(), &q));

    TEST_ASSERT_EQUAL_size_t(0, trs_queue_len(q));
    TEST_ASSERT_EQUAL_size_t(16, trs_queue_cap(q));

    trs_queue_drop(q);
}

// the queue takes them in the order they are written, so the first one written is the
// first one served
static void test_from_data_keeps_arrival_order() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);

    assert_elems(q, SPREAD, SPREAD_LEN);
    TEST_ASSERT_EQUAL_INT32(SPREAD[0], *TRS_QUEUE_FRONT_AS(int32_t, q));
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *TRS_QUEUE_BACK_AS(int32_t, q));

    trs_queue_drop(q);
}

static void test_from_data_empty_stays_empty() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_FROM_DATA(int32_t, nullptr, 0, trs_al_default(), &q));

    TEST_ASSERT_EQUAL_size_t(0, trs_queue_len(q));

    trs_queue_drop(q);
}

static void test_from_span_copies_the_view() {
    constexpr int32_t src[4] = {9, 8, 7, 6};

    trs_Queue *q = nullptr;
    TRS_TEST_OK(trs_queue_from_span(TRS_SPAN_FROM_DATA(int32_t, src, 4), trs_al_default(), &q));

    assert_elems(q, src, 4);

    trs_queue_drop(q);
}

// an elem wider than a word must travel whole, not by its first field
static void test_from_data_copies_whole_elems() {
    constexpr Pair src[3] = {{1, 10}, {2, 20}, {3, 30}};

    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_FROM_DATA(Pair, src, 3, trs_al_default(), &q));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), trs_queue_elem_size(q));

    Pair got[3];
    trs_queue_copy_to_span(q, TRS_SPAN_FROM_DATA_MUT(Pair, got, 3));
    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_INT64(src[i].a, got[i].a);
        TEST_ASSERT_EQUAL_INT64(src[i].b, got[i].b);
    }

    trs_queue_drop(q);
}

static void test_drop_null_is_noop() {
    trs_queue_drop(nullptr);
}

// three blocks go into a filled queue — the deque, its buffer, and the header that hides
// the deque — and drop must hand back all three. The default allocator would say nothing
// about it, so the count comes from a probe
static void test_drop_hands_back_everything_it_took() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_FROM_DATA(int32_t, SPREAD, SPREAD_LEN, &al, &q));
    TEST_ASSERT_EQUAL_size_t(3, probe.live);

    trs_queue_drop(q);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
    TEST_ASSERT_EQUAL_size_t(3, probe.dealloc_calls);
}

/* ========== fifo ========== */

static void test_pushes_leave_in_the_order_they_arrived() {
    trs_Queue *q = make_queue(SPREAD, SPREAD_LEN);

    assert_drains(q, SPREAD, SPREAD_LEN);

    trs_queue_drop(q);
}

// growth relocates every elem, so the order has to survive a move of the whole ring —
// this queue outgrows its first block several times over
static void test_order_survives_growth() {
    int32_t want[64];
    for (int32_t i = 0; i < 64; ++i) {
        want[i] = i * 3;
    }

    trs_Queue *q = make_queue(want, 64);

    TEST_ASSERT_TRUE(trs_queue_cap(q) >= 64);
    assert_drains(q, want, 64);

    trs_queue_drop(q);
}

// the ring is reused as it empties: pushing and popping in step keeps the queue short
// while far more elems than it holds pass through, so the buffer wraps many times over
static void test_pushing_and_popping_in_step_stays_in_order() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW(int32_t, trs_al_default(), &q));

    push_int(q, 0);
    push_int(q, 1);
    push_int(q, 2);

    for (int32_t i = 3; i < 100; ++i) {
        TEST_ASSERT_EQUAL_INT32(i - 3, *TRS_QUEUE_FRONT_AS(int32_t, q));
        trs_queue_pop(q);
        push_int(q, i);

        TEST_ASSERT_EQUAL_size_t(3, trs_queue_len(q));
        TEST_ASSERT_EQUAL_INT32(i, *TRS_QUEUE_BACK_AS(int32_t, q));
    }

    constexpr int32_t want[3] = {97, 98, 99};
    assert_drains(q, want, 3);

    trs_queue_drop(q);
}

static void test_pushes_after_a_full_drain_are_ordered_again() {
    trs_Queue *q = make_queue(SPREAD, SPREAD_LEN);
    assert_drains(q, SPREAD, SPREAD_LEN);

    constexpr int32_t again[4] = {41, 42, 43, 44};
    for (size_t i = 0; i < 4; ++i) {
        push_int(q, again[i]);
    }
    assert_drains(q, again, 4);

    trs_queue_drop(q);
}

// equal elems are not one elem: every copy pushed must come back
static void test_duplicates_all_come_back() {
    constexpr int32_t src[6] = {7, 7, 7, 7, 7, 7};

    trs_Queue *q = make_queue(src, 6);

    assert_drains(q, src, 6);

    trs_queue_drop(q);
}

static void test_a_wrapped_queue_drains_in_order() {
    trs_Queue *q = make_wrapped();

    constexpr int32_t want[4] = {10, 20, 30, 40};
    assert_drains(q, want, 4);

    trs_queue_drop(q);
}

// the elem that has to move is the one that wrapped, so growing a split ring is where
// the order is easiest to lose
static void test_a_wrapped_queue_keeps_its_order_through_growth() {
    trs_Queue *q = make_wrapped();

    push_int(q, 50);
    TEST_ASSERT_TRUE(trs_queue_cap(q) > 4);

    constexpr int32_t want[5] = {10, 20, 30, 40, 50};
    assert_drains(q, want, 5);

    trs_queue_drop(q);
}

/* ========== access ========== */

static void test_front_reads_without_removing() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);

    TEST_ASSERT_EQUAL_INT32(SPREAD[0], *TRS_QUEUE_FRONT_AS(int32_t, q));
    TEST_ASSERT_EQUAL_INT32(SPREAD[0], *TRS_QUEUE_FRONT_AS(int32_t, q));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_queue_len(q));

    trs_queue_drop(q);
}

static void test_back_follows_the_last_push() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW(int32_t, trs_al_default(), &q));

    for (int32_t i = 0; i < 8; ++i) {
        push_int(q, i);
        TEST_ASSERT_EQUAL_INT32(i, *TRS_QUEUE_BACK_AS(int32_t, q));
        TEST_ASSERT_EQUAL_INT32(0, *TRS_QUEUE_FRONT_AS(int32_t, q));
    }

    trs_queue_drop(q);
}

// there is no order over the elems to break, so writing through an end is legal —
// the queue only rules where they enter and leave
static void test_the_ends_are_writable() {
    trs_Queue *q = make_wrapped();

    *TRS_QUEUE_FRONT_MUT_AS(int32_t, q) = -1;
    *TRS_QUEUE_BACK_MUT_AS(int32_t, q) = -4;

    constexpr int32_t want[4] = {-1, 20, 30, -4};
    assert_elems(q, want, 4);

    trs_queue_drop(q);
}

// on a queue of one the two ends are the same elem
static void test_front_and_back_meet_on_a_single_elem() {
    trs_Queue *q = make_queue_from(SPREAD, 1);

    TEST_ASSERT_EQUAL_PTR(trs_queue_front(q), trs_queue_back(q));
    TEST_ASSERT_EQUAL_PTR(trs_queue_front(q), trs_queue_front_mut(q));

    trs_queue_drop(q);
}

/* ========== info ========== */

static void test_len_follows_push_and_pop() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW(int32_t, trs_al_default(), &q));

    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_size_t(i, trs_queue_len(q));
        push_int(q, (int32_t) i);
    }
    for (size_t i = 5; i > 0; --i) {
        TEST_ASSERT_EQUAL_size_t(i, trs_queue_len(q));
        trs_queue_pop(q);
    }
    TEST_ASSERT_EQUAL_size_t(0, trs_queue_len(q));

    trs_queue_drop(q);
}

// popping hands nothing back to the allocator: the room stays for the next push
static void test_pop_leaves_the_capacity_alone() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);
    const size_t cap = trs_queue_cap(q);

    trs_queue_pop(q);
    trs_queue_pop(q);

    TEST_ASSERT_EQUAL_size_t(cap, trs_queue_cap(q));

    trs_queue_drop(q);
}

/* ========== mods ========== */

static void test_clear_empties_without_giving_back_the_room() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);
    const size_t cap = trs_queue_cap(q);

    trs_queue_clear(q);

    TEST_ASSERT_EQUAL_size_t(0, trs_queue_len(q));
    TEST_ASSERT_EQUAL_size_t(cap, trs_queue_cap(q));

    trs_queue_drop(q);
}

// clearing a split ring must not leave the front pointing into the middle of it
static void test_clear_leaves_a_usable_queue() {
    trs_Queue *q = make_wrapped();

    trs_queue_clear(q);

    constexpr int32_t again[3] = {1, 2, 3};
    for (size_t i = 0; i < 3; ++i) {
        push_int(q, again[i]);
    }
    assert_drains(q, again, 3);

    trs_queue_drop(q);
}

static void test_reserve_grows_the_room_only() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);

    TRS_TEST_OK(trs_queue_reserve(q, 100));

    TEST_ASSERT_TRUE(trs_queue_cap(q) >= 100);
    assert_elems(q, SPREAD, SPREAD_LEN);

    trs_queue_drop(q);
}

static void test_reserve_below_the_capacity_changes_nothing() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);
    const size_t cap = trs_queue_cap(q);

    TRS_TEST_OK(trs_queue_reserve(q, 1));

    TEST_ASSERT_EQUAL_size_t(cap, trs_queue_cap(q));

    trs_queue_drop(q);
}

static void test_shrink_to_fit_keeps_the_order() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);
    TRS_TEST_OK(trs_queue_reserve(q, 100));

    TRS_TEST_OK(trs_queue_shrink_to_fit(q));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_queue_cap(q));
    assert_elems(q, SPREAD, SPREAD_LEN);

    trs_queue_drop(q);
}

// shrinking a split ring has to unwrap it on the way into the smaller block
static void test_shrink_to_fit_unwraps_what_it_moves() {
    trs_Queue *q = make_wrapped();
    trs_queue_pop(q);

    TRS_TEST_OK(trs_queue_shrink_to_fit(q));

    TEST_ASSERT_EQUAL_size_t(3, trs_queue_cap(q));
    constexpr int32_t want[3] = {20, 30, 40};
    assert_elems(q, want, 3);

    trs_queue_drop(q);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    trs_Queue *src = make_queue_from(SPREAD, SPREAD_LEN);

    trs_Queue *dst = nullptr;
    TRS_TEST_OK(trs_queue_copy(src, &dst));

    trs_queue_pop(dst);
    push_int(dst, 777);

    assert_elems(src, SPREAD, SPREAD_LEN);
    TEST_ASSERT_EQUAL_INT32(SPREAD[1], *TRS_QUEUE_FRONT_AS(int32_t, dst));
    TEST_ASSERT_EQUAL_INT32(777, *TRS_QUEUE_BACK_AS(int32_t, dst));

    trs_queue_drop(src);
    trs_queue_drop(dst);
}

static void test_copy_inherits_the_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Queue *src = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, arena, &src, 5, 1, 3));

    trs_Queue *dst = nullptr;
    TRS_TEST_OK(trs_queue_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_queue_al(dst));
    TEST_ASSERT_EQUAL_INT32(5, *TRS_QUEUE_FRONT_AS(int32_t, dst));

    trs_queue_drop(src);
    trs_queue_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_copy_with_builds_on_the_given_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Queue *src = make_wrapped();

    trs_Queue *dst = nullptr;
    TRS_TEST_OK(trs_queue_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_queue_al(dst));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_queue_al(src));
    TEST_ASSERT_TRUE(trs_queue_eq(src, dst));

    // the header goes to the same allocator as the elems, so the copy outlives the source
    trs_queue_drop(src);
    TEST_ASSERT_EQUAL_INT32(10, *TRS_QUEUE_FRONT_AS(int32_t, dst));

    trs_queue_drop(dst);
    trs_al_arena_drop(arena);
}

// the blocks are asked of the allocator the copy is going to, not of the source's
static void test_copy_with_reports_an_exhausted_target_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_Queue *src = make_queue_from(SPREAD, SPREAD_LEN);

    trs_Queue *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_queue_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_queue_len(src));

    trs_queue_drop(src);
    trs_al_arena_drop(arena);
}

static void test_move_assign_hands_over_the_contents_on_one_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Queue *src = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, &al, &src, 1, 2, 3));

    trs_Queue *dst = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, &al, &dst, 9));

    const size_t requests = trs_test_probe_requests(&probe);
    TRS_TEST_OK(trs_queue_move_assign(src, dst));

    // nothing was asked of the allocator: the deque's ring changed hands
    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    TEST_ASSERT_EQUAL_size_t(3, trs_queue_len(dst));
    TEST_ASSERT_EQUAL_INT32(1, *TRS_QUEUE_FRONT_AS(int32_t, dst));
    TEST_ASSERT_EQUAL_size_t(0, trs_queue_len(src));

    trs_queue_drop(src);
    trs_queue_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_move_assign_across_allocators_empties_the_source() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    // a split ring must arrive in queue order, exactly as in copy
    trs_Queue *src = make_wrapped();

    trs_Queue *dst = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, arena, &dst, 9));

    TRS_TEST_OK(trs_queue_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(4, trs_queue_len(dst));
    TEST_ASSERT_EQUAL_INT32(10, *TRS_QUEUE_FRONT_AS(int32_t, dst));
    TEST_ASSERT_EQUAL_PTR(arena, trs_queue_al(dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_queue_len(src));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_queue_al(src));

    trs_queue_drop(src);
    trs_queue_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_across_allocators_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Queue *dst = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, arena, &dst, 9));
    trs_test_arena_leave(arena, 0);

    trs_Queue *src = make_queue_from(SPREAD, SPREAD_LEN);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_queue_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_queue_len(src));
    TEST_ASSERT_EQUAL_size_t(1, trs_queue_len(dst));
    TEST_ASSERT_EQUAL_INT32(9, *TRS_QUEUE_FRONT_AS(int32_t, dst));

    trs_queue_drop(src);
    trs_queue_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_of_itself_changes_nothing() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);

    TRS_TEST_OK(trs_queue_move_assign(q, q));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_queue_len(q));
    TEST_ASSERT_EQUAL_INT32(SPREAD[0], *TRS_QUEUE_FRONT_AS(int32_t, q));

    trs_queue_drop(q);
}

// the copy of a split ring must come out in queue order, not in buffer order
static void test_copy_of_a_wrapped_queue_keeps_the_order() {
    trs_Queue *src = make_wrapped();

    trs_Queue *dst = nullptr;
    TRS_TEST_OK(trs_queue_copy(src, &dst));

    constexpr int32_t want[4] = {10, 20, 30, 40};
    assert_drains(dst, want, 4);

    trs_queue_drop(src);
    trs_queue_drop(dst);
}

static void test_copy_of_empty_stays_empty() {
    trs_Queue *src = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW(int32_t, trs_al_default(), &src));

    trs_Queue *dst = nullptr;
    TRS_TEST_OK(trs_queue_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_queue_len(dst));

    trs_queue_drop(src);
    trs_queue_drop(dst);
}

static void test_copy_assign_overwrites_the_target() {
    trs_Queue *src = make_queue_from(SPREAD, SPREAD_LEN);
    trs_Queue *dst = make_wrapped();

    TRS_TEST_OK(trs_queue_copy_assign(src, dst));

    assert_elems(dst, SPREAD, SPREAD_LEN);
    assert_elems(src, SPREAD, SPREAD_LEN);

    trs_queue_drop(src);
    trs_queue_drop(dst);
}

static void test_copy_assign_self_is_noop() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);

    TRS_TEST_OK(trs_queue_copy_assign(q, q));

    assert_elems(q, SPREAD, SPREAD_LEN);

    trs_queue_drop(q);
}

/* ========== copy to span ========== */

static void test_copy_to_span_writes_front_to_back() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);

    int32_t got[SPREAD_LEN];
    trs_queue_copy_to_span(q, TRS_SPAN_FROM_DATA_MUT(int32_t, got, SPREAD_LEN));

    TEST_ASSERT_EQUAL_INT32_ARRAY(SPREAD, got, SPREAD_LEN);
    assert_elems(q, SPREAD, SPREAD_LEN);

    trs_queue_drop(q);
}

// the contents are at most two runs in the buffer and exactly one run here
static void test_copy_to_span_of_a_wrapped_queue_is_in_order() {
    trs_Queue *q = make_wrapped();

    int32_t got[4];
    trs_queue_copy_to_span(q, TRS_SPAN_FROM_DATA_MUT(int32_t, got, 4));

    constexpr int32_t want[4] = {10, 20, 30, 40};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, got, 4);

    trs_queue_drop(q);
}

static void test_copy_to_span_of_empty_writes_nothing() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW(int32_t, trs_al_default(), &q));

    int32_t got[2] = {11, 22};
    trs_queue_copy_to_span(q, TRS_SPAN_FROM_DATA_MUT(int32_t, got, 0));

    TEST_ASSERT_EQUAL_INT32(11, got[0]);
    TEST_ASSERT_EQUAL_INT32(22, got[1]);

    trs_queue_drop(q);
}

// the whole point of the one way bridge: algo works on the copy, and the queue keeps
// the arrival order that gives it its meaning
static void test_the_copy_reaches_algo_and_the_queue_is_untouched() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);

    int32_t got[SPREAD_LEN];
    const trs_SpanMut s = TRS_SPAN_FROM_DATA_MUT(int32_t, got, SPREAD_LEN);
    trs_queue_copy_to_span(q, s);

    TEST_ASSERT_EQUAL_size_t(2, trs_span_max_elem(trs_span_mut_to_span(s), trs_cmp_i32));

    trs_span_sort(s, trs_cmp_i32);
    TEST_ASSERT_TRUE(trs_span_is_sorted(trs_span_mut_to_span(s), trs_cmp_i32));

    assert_elems(q, SPREAD, SPREAD_LEN);

    trs_queue_drop(q);
}

/* ========== swap ========== */

static void test_swap_exchanges_the_elems() {
    trs_Queue *a = nullptr;
    trs_Queue *b = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, trs_al_default(), &a, 1, 2, 3));
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, trs_al_default(), &b, 10, 20));

    trs_queue_swap(a, b);

    constexpr int32_t want_a[2] = {10, 20};
    constexpr int32_t want_b[3] = {1, 2, 3};
    assert_elems(a, want_a, 2);
    assert_elems(b, want_b, 3);

    trs_queue_drop(a);
    trs_queue_drop(b);
}

static void test_swap_self_is_noop() {
    trs_Queue *q = make_queue_from(SPREAD, SPREAD_LEN);

    trs_queue_swap(q, q);

    assert_elems(q, SPREAD, SPREAD_LEN);

    trs_queue_drop(q);
}

/* ========== failures ========== */

static void test_new_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_Queue *q = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_QUEUE_NEW(int32_t, arena, &q));
    TEST_ASSERT_NULL(q);

    trs_al_arena_drop(arena);
}

static void test_from_data_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 128);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Queue *q = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_QUEUE_FROM_DATA(int32_t, SPREAD, 1000, arena, &q));
    TEST_ASSERT_NULL(q);

    trs_al_arena_drop(arena);
}

// a refused push must leave the queue exactly as it was — not a ring with a hole in it
static void test_push_reports_an_exhausted_arena_and_changes_nothing() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, arena, &q, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    constexpr int32_t val = 100;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_queue_push(q, &val));

    constexpr int32_t want[3] = {5, 1, 3};
    assert_elems(q, want, 3);

    trs_queue_drop(q);
    trs_al_arena_drop(arena);
}

static void test_copy_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Queue *src = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, arena, &src, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    trs_Queue *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_queue_copy(src, &dst));
    TEST_ASSERT_NULL(dst);

    trs_queue_drop(src);
    trs_al_arena_drop(arena);
}

static void test_reserve_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, arena, &q, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_queue_reserve(q, 1000));

    constexpr int32_t want[3] = {5, 1, 3};
    assert_elems(q, want, 3);

    trs_queue_drop(q);
    trs_al_arena_drop(arena);
}

// The queue is built in two steps, the deque and then the header that hides it. When the
// second one is refused the first must not be stranded: the probe counts what is still
// live, and an arena would hide the leak because it frees everything at once
static void test_a_refused_header_frees_the_deque() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 1);

    trs_Queue *q = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_QUEUE_NEW(int32_t, &al, &q));

    TEST_ASSERT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the same, one allocation later: from_data takes a buffer as well
static void test_a_refused_header_frees_a_filled_deque() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 2);

    trs_Queue *q = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_QUEUE_FROM_DATA(int32_t, SPREAD, SPREAD_LEN, &al, &q));

    TEST_ASSERT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== compare ========== */

static void test_eq_matches_the_same_elems() {
    constexpr int32_t src[3] = {7, 8, 9};

    trs_Queue *a = make_queue(src, 3);
    trs_Queue *b = make_queue_from(src, 3);

    TEST_ASSERT_TRUE(trs_queue_eq(a, a));
    TEST_ASSERT_TRUE(trs_queue_eq(a, b));
    TEST_ASSERT_TRUE(trs_queue_eq(b, a));
    TEST_ASSERT_TRUE(trs_queue_eq_by(a, b, trs_eq_i32));

    trs_queue_drop(a);
    trs_queue_drop(b);
}

static void test_eq_parts_one_differing_elem() {
    constexpr int32_t lhs[3] = {7, 8, 9};
    constexpr int32_t rhs[3] = {7, 8, 99};

    trs_Queue *a = make_queue(lhs, 3);
    trs_Queue *b = make_queue(rhs, 3);

    TEST_ASSERT_FALSE(trs_queue_eq(a, b));
    TEST_ASSERT_FALSE(trs_queue_eq_by(a, b, trs_eq_i32));

    trs_queue_drop(a);
    trs_queue_drop(b);
}

static void test_eq_parts_different_lengths() {
    constexpr int32_t src[3] = {7, 8, 9};

    trs_Queue *a = make_queue(src, 3);
    trs_Queue *shorter = make_queue(src, 2);

    TEST_ASSERT_FALSE(trs_queue_eq(a, shorter));
    TEST_ASSERT_FALSE(trs_queue_eq(shorter, a));

    trs_queue_drop(a);
    trs_queue_drop(shorter);
}

static void test_eq_of_two_empties() {
    constexpr int32_t src[1] = {7};

    trs_Queue *a = make_queue(src, 0);
    trs_Queue *b = make_queue_from(src, 0);
    trs_Queue *one = make_queue(src, 1);

    TEST_ASSERT_TRUE(trs_queue_eq(a, b));
    TEST_ASSERT_TRUE(trs_queue_eq_by(a, b, trs_eq_i32));
    TEST_ASSERT_FALSE(trs_queue_eq(a, one));

    trs_queue_drop(a);
    trs_queue_drop(b);
    trs_queue_drop(one);
}

// the ring under the queue may be split or not; the queue order is what is compared
static void test_eq_ignores_where_the_ring_starts() {
    constexpr int32_t want[4] = {10, 20, 30, 40};

    trs_Queue *straight = make_queue_from(want, 4);
    trs_Queue *wrapped = make_wrapped();

    TEST_ASSERT_TRUE(trs_queue_eq(straight, wrapped));
    TEST_ASSERT_TRUE(trs_queue_eq(wrapped, straight));
    TEST_ASSERT_TRUE(trs_queue_eq_by(wrapped, straight, trs_eq_i32));

    trs_queue_drop(straight);
    trs_queue_drop(wrapped);
}

// a queue is compared front to back: the same elems in another order are another queue
static void test_eq_is_order_sensitive() {
    constexpr int32_t src[3] = {7, 8, 9};

    trs_Queue *a = make_queue(src, 3);
    trs_Queue *b = make_queue(src, 3);

    const int32_t front = *TRS_QUEUE_FRONT_AS(int32_t, b);
    trs_queue_pop(b);
    push_int(b, front);

    TEST_ASSERT_EQUAL_size_t(trs_queue_len(a), trs_queue_len(b));
    TEST_ASSERT_FALSE(trs_queue_eq(a, b));

    trs_queue_drop(a);
    trs_queue_drop(b);
}

static void test_eq_by_asks_the_equality() {
    constexpr Pair lhs[2] = {{1, 10}, {2, 20}};
    constexpr Pair rhs[2] = {{1, 70}, {2, 80}};

    trs_Queue *a = nullptr;
    trs_Queue *b = nullptr;
    TRS_TEST_OK(TRS_QUEUE_FROM_DATA(Pair, lhs, 2, trs_al_default(), &a));
    TRS_TEST_OK(TRS_QUEUE_FROM_DATA(Pair, rhs, 2, trs_al_default(), &b));

    TEST_ASSERT_FALSE(trs_queue_eq(a, b));
    TEST_ASSERT_TRUE(trs_queue_eq_by(a, b, trs_test_pair_eq_a));

    trs_queue_drop(a);
    trs_queue_drop(b);
}

/* ========== into ========== */

// the deque was there all along, ring and all: taking it copies nothing
static void test_into_deque_hands_the_elems_over() {
    constexpr int32_t want[4] = {10, 20, 30, 40};
    trs_Queue *q = make_wrapped();

    const size_t cap = trs_queue_cap(q);
    trs_Deque *d = trs_queue_into_deque(q);

    TEST_ASSERT_EQUAL_size_t(4, trs_deque_len(d));
    TEST_ASSERT_EQUAL_size_t(cap, trs_deque_cap(d));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_deque_al(d));

    // the elems keep their queue order even though the ring underneath is split
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_INT32(want[i], *TRS_DEQUE_GET_AS(int32_t, d, i));
    }

    trs_deque_drop(d);
}

static void test_into_deque_of_an_empty_queue() {
    constexpr int32_t src[1] = {7};
    trs_Queue *q = make_queue(src, 0);

    trs_Deque *d = trs_queue_into_deque(q);

    TEST_ASSERT_EQUAL_size_t(0, trs_deque_len(d));

    trs_deque_drop(d);
}

static void test_into_deque_releases_the_header_alone() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_NEW(int32_t, &al, &q));

    // the last block the constructor took is the adapter's own header, so this is what
    // into has to hand back, and with the size it was taken as
    const size_t header = probe.last_alloc_size;

    TRS_TEST_OK(TRS_QUEUE_PUSH(int32_t, q, 1));
    const size_t live = probe.live;

    trs_Deque *d = trs_queue_into_deque(q);

    TEST_ASSERT_EQUAL_size_t(live - 1, probe.live);
    TEST_ASSERT_EQUAL_size_t(header, probe.last_dealloc_size);

    trs_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== print ========== */

// a printer writes to a stream, so a case has to read one back. tmpfile is the portable
// way, the same one test/core/test_print.c takes
static void assert_prints(const char *expected, const trs_Queue *q) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);

    trs_queue_fprint(q, stream, trs_fprint_i32);
    rewind(stream);

    char buf[128];
    const size_t n = fread(buf, 1, sizeof buf - 1, stream);
    buf[n] = '\0';
    fclose(stream);

    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_fprint_writes_the_elems() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, trs_al_default(), &q, 5, 3, 1));

    assert_prints("[5, 3, 1]\n", q);

    trs_queue_drop(q);
}

static void test_fprint_of_a_single_elem_has_no_separator() {
    trs_Queue *q = nullptr;
    TRS_TEST_OK(TRS_QUEUE_OF(int32_t, trs_al_default(), &q, 7));

    assert_prints("[7]\n", q);

    trs_queue_drop(q);
}

static void test_fprint_of_an_empty_queue() {
    trs_Queue *q = make_queue(nullptr, 0);

    assert_prints("[]\n", q);

    trs_queue_drop(q);
}

// the stdout twin takes no stream, and C has no portable way to capture one and give it
// back — so a case can only say that it runs and reaches the same printer
static void test_print_writes_to_stdout() {
    constexpr int32_t src[3] = {1, 2, 3};
    trs_Queue *q = make_queue(src, 3);

    trs_queue_print(q, trs_fprint_i32);

    trs_queue_drop(q);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_empty);
    RUN_TEST(test_new_cap_reserves_without_length);
    RUN_TEST(test_from_data_keeps_arrival_order);
    RUN_TEST(test_from_data_empty_stays_empty);
    RUN_TEST(test_from_span_copies_the_view);
    RUN_TEST(test_from_data_copies_whole_elems);
    RUN_TEST(test_drop_null_is_noop);
    RUN_TEST(test_drop_hands_back_everything_it_took);

    RUN_TEST(test_pushes_leave_in_the_order_they_arrived);
    RUN_TEST(test_order_survives_growth);
    RUN_TEST(test_pushing_and_popping_in_step_stays_in_order);
    RUN_TEST(test_pushes_after_a_full_drain_are_ordered_again);
    RUN_TEST(test_duplicates_all_come_back);
    RUN_TEST(test_a_wrapped_queue_drains_in_order);
    RUN_TEST(test_a_wrapped_queue_keeps_its_order_through_growth);

    RUN_TEST(test_front_reads_without_removing);
    RUN_TEST(test_back_follows_the_last_push);
    RUN_TEST(test_the_ends_are_writable);
    RUN_TEST(test_front_and_back_meet_on_a_single_elem);

    RUN_TEST(test_len_follows_push_and_pop);
    RUN_TEST(test_pop_leaves_the_capacity_alone);

    RUN_TEST(test_clear_empties_without_giving_back_the_room);
    RUN_TEST(test_clear_leaves_a_usable_queue);
    RUN_TEST(test_reserve_grows_the_room_only);
    RUN_TEST(test_reserve_below_the_capacity_changes_nothing);
    RUN_TEST(test_shrink_to_fit_keeps_the_order);
    RUN_TEST(test_shrink_to_fit_unwraps_what_it_moves);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_inherits_the_allocator);
    RUN_TEST(test_copy_with_builds_on_the_given_allocator);
    RUN_TEST(test_copy_with_reports_an_exhausted_target_arena);
    RUN_TEST(test_move_assign_hands_over_the_contents_on_one_allocator);
    RUN_TEST(test_move_assign_across_allocators_empties_the_source);
    RUN_TEST(test_move_assign_across_allocators_reports_an_exhausted_arena);
    RUN_TEST(test_move_assign_of_itself_changes_nothing);
    RUN_TEST(test_copy_of_a_wrapped_queue_keeps_the_order);
    RUN_TEST(test_copy_of_empty_stays_empty);
    RUN_TEST(test_copy_assign_overwrites_the_target);
    RUN_TEST(test_copy_assign_self_is_noop);

    RUN_TEST(test_copy_to_span_writes_front_to_back);
    RUN_TEST(test_copy_to_span_of_a_wrapped_queue_is_in_order);
    RUN_TEST(test_copy_to_span_of_empty_writes_nothing);
    RUN_TEST(test_the_copy_reaches_algo_and_the_queue_is_untouched);

    RUN_TEST(test_swap_exchanges_the_elems);
    RUN_TEST(test_swap_self_is_noop);

    RUN_TEST(test_new_reports_an_exhausted_arena);
    RUN_TEST(test_from_data_reports_an_exhausted_arena);
    RUN_TEST(test_push_reports_an_exhausted_arena_and_changes_nothing);
    RUN_TEST(test_copy_reports_an_exhausted_arena);
    RUN_TEST(test_reserve_reports_an_exhausted_arena);
    RUN_TEST(test_a_refused_header_frees_the_deque);
    RUN_TEST(test_a_refused_header_frees_a_filled_deque);


    RUN_TEST(test_eq_matches_the_same_elems);
    RUN_TEST(test_eq_parts_one_differing_elem);
    RUN_TEST(test_eq_parts_different_lengths);
    RUN_TEST(test_eq_of_two_empties);
    RUN_TEST(test_eq_ignores_where_the_ring_starts);
    RUN_TEST(test_eq_is_order_sensitive);
    RUN_TEST(test_eq_by_asks_the_equality);


    RUN_TEST(test_into_deque_hands_the_elems_over);
    RUN_TEST(test_into_deque_of_an_empty_queue);
    RUN_TEST(test_into_deque_releases_the_header_alone);

    RUN_TEST(test_fprint_writes_the_elems);
    RUN_TEST(test_fprint_of_a_single_elem_has_no_separator);
    RUN_TEST(test_fprint_of_an_empty_queue);
    RUN_TEST(test_print_writes_to_stdout);

    return UNITY_END();
}
