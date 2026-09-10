#include "trs/ds/deque.h"
#include "trs/algo/sort.h"
#include "trs/alloc/arena.h"
#include "trs/alloc/default.h"
#include "trs/core/cmp.h"
#include "trs/core/print.h"
#include "trs/core/util.h"

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

// the contents wrap exactly when the front sits at a higher address than the back.
// Both point into the same block, so the comparison is meaningful — and it lets a
// test state that it really is exercising a split ring instead of assuming it
[[nodiscard]]
static bool wraps(const trs_Deque *d) {
    return (const unsigned char *) trs_deque_front(d) > (const unsigned char *) trs_deque_back(d);
}

static void assert_elems(const trs_Deque *d, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_deque_len(d));

    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_INT32(want[i], *TRS_DEQUE_GET_AS(int32_t, d, i));
    }
}

static void push_back_int(trs_Deque *d, int32_t val) {
    TRS_TEST_OK(trs_deque_push_back(d, &val));
}

static void push_front_int(trs_Deque *d, int32_t val) {
    TRS_TEST_OK(trs_deque_push_front(d, &val));
}

// int32_t deque holding 0, 1, ... len-1, filled from the back
[[nodiscard]]
static trs_Deque *make_deque(size_t len) {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW(int32_t, trs_al_default(), &d));

    for (size_t i = 0; i < len; ++i) {
        push_back_int(d, (int32_t) i);
    }
    return d;
}

// {10, 20, 30, 40} in a ring of exactly four slots that is guaranteed to be split:
// two elems are pushed off the front and the same number wrapped around onto the back
[[nodiscard]]
static trs_Deque *make_wrapped(void) {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_CAP(int32_t, 4, trs_al_default(), &d));

    push_back_int(d, 1);
    push_back_int(d, 2);
    push_back_int(d, 10);
    push_back_int(d, 20);

    trs_deque_pop_front(d);
    trs_deque_pop_front(d);

    push_back_int(d, 30);
    push_back_int(d, 40);

    TEST_ASSERT_EQUAL_size_t(4, trs_deque_cap(d));
    TEST_ASSERT_TRUE(wraps(d));

    return d;
}

/* ========== lifetime ========== */

static void test_new_starts_empty_and_unallocated() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW(int32_t, trs_al_default(), &d));

    TEST_ASSERT_EQUAL_size_t(0, trs_deque_len(d));
    TEST_ASSERT_EQUAL_size_t(0, trs_deque_cap(d));
    TEST_ASSERT_EQUAL_size_t(0, trs_deque_bytes(d));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_deque_elem_size(d));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_deque_al(d));

    trs_deque_drop(d);
}

static void test_new_len_zeroes_its_elems() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_LEN(int32_t, 3, trs_al_default(), &d));

    assert_elems(d, (int32_t[]){0, 0, 0}, 3);
    TEST_ASSERT_EQUAL_size_t(3 * sizeof(int32_t), trs_deque_bytes(d));

    trs_deque_drop(d);
}

static void test_new_cap_reserves_without_length() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_CAP(int32_t, 8, trs_al_default(), &d));

    TEST_ASSERT_EQUAL_size_t(0, trs_deque_len(d));
    TEST_ASSERT_EQUAL_size_t(8, trs_deque_cap(d));

    trs_deque_drop(d);
}

static void test_of_keeps_the_order() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_OF(int32_t, trs_al_default(), &d, 5, 6, 7));

    assert_elems(d, (int32_t[]){5, 6, 7}, 3);
    TEST_ASSERT_FALSE(wraps(d));

    trs_deque_drop(d);
}

static void test_from_span_copies_the_elems() {
    constexpr int32_t src[4] = {9, 8, 7, 6};

    trs_Deque *d = nullptr;
    TRS_TEST_OK(trs_deque_from_span(TRS_SPAN_FROM_DATA(int32_t, src, 4), trs_al_default(), &d));

    assert_elems(d, (int32_t[]){9, 8, 7, 6}, 4);

    trs_deque_drop(d);
}

static void test_drop_of_null_is_a_no_op() {
    trs_deque_drop(nullptr);
}

/* ========== ends ========== */

static void test_push_back_appends() {
    trs_Deque *d = make_deque(0);

    push_back_int(d, 1);
    push_back_int(d, 2);
    push_back_int(d, 3);

    assert_elems(d, (int32_t[]){1, 2, 3}, 3);

    trs_deque_drop(d);
}

// the same calls from the other end come out reversed — that is the whole difference
static void test_push_front_prepends() {
    trs_Deque *d = make_deque(0);

    push_front_int(d, 1);
    push_front_int(d, 2);
    push_front_int(d, 3);

    assert_elems(d, (int32_t[]){3, 2, 1}, 3);

    trs_deque_drop(d);
}

static void test_pushes_from_both_ends_meet_in_the_middle() {
    trs_Deque *d = make_deque(0);

    push_back_int(d, 0);
    push_front_int(d, -1);
    push_back_int(d, 1);
    push_front_int(d, -2);
    push_back_int(d, 2);

    assert_elems(d, (int32_t[]){-2, -1, 0, 1, 2}, 5);

    trs_deque_drop(d);
}

static void test_pop_front_and_pop_back_take_from_their_own_ends() {
    trs_Deque *d = make_deque(5);

    trs_deque_pop_front(d);
    trs_deque_pop_back(d);

    assert_elems(d, (int32_t[]){1, 2, 3}, 3);

    trs_deque_drop(d);
}

static void test_first_and_last_follow_the_ends() {
    trs_Deque *d = make_deque(3);

    TEST_ASSERT_EQUAL_INT32(0, *TRS_DEQUE_FRONT_AS(int32_t, d));
    TEST_ASSERT_EQUAL_INT32(2, *TRS_DEQUE_BACK_AS(int32_t, d));

    push_front_int(d, 9);
    push_back_int(d, 8);

    TEST_ASSERT_EQUAL_INT32(9, *TRS_DEQUE_FRONT_AS(int32_t, d));
    TEST_ASSERT_EQUAL_INT32(8, *TRS_DEQUE_BACK_AS(int32_t, d));

    // a single elem is both ends at once
    trs_Deque *one = make_deque(1);
    TEST_ASSERT_EQUAL_PTR(trs_deque_front(one), trs_deque_back(one));

    trs_deque_drop(one);
    trs_deque_drop(d);
}

/* ========== the ring ========== */

static void test_the_ring_wraps_and_get_stays_relative_to_the_front() {
    trs_Deque *d = make_wrapped();

    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);
    TEST_ASSERT_EQUAL_INT32(10, *TRS_DEQUE_FRONT_AS(int32_t, d));
    TEST_ASSERT_EQUAL_INT32(40, *TRS_DEQUE_BACK_AS(int32_t, d));

    trs_deque_drop(d);
}

// the classic ring bug: growing a split ring must unroll it, not copy the block
static void test_growth_while_wrapped_keeps_the_order() {
    trs_Deque *d = make_wrapped();

    push_back_int(d, 50);

    TEST_ASSERT_TRUE(trs_deque_cap(d) > 4);
    TEST_ASSERT_FALSE(wraps(d));
    assert_elems(d, (int32_t[]){10, 20, 30, 40, 50}, 5);

    trs_deque_drop(d);
}

// the same growth from the other end: the new front must not land inside the old run
static void test_growth_while_wrapped_from_the_front_keeps_the_order() {
    trs_Deque *d = make_wrapped();

    push_front_int(d, 5);

    TEST_ASSERT_TRUE(trs_deque_cap(d) > 4);
    assert_elems(d, (int32_t[]){5, 10, 20, 30, 40}, 5);

    trs_deque_drop(d);
}

static void test_push_front_on_an_empty_deque_allocates() {
    trs_Deque *d = make_deque(0);

    push_front_int(d, 42);

    TEST_ASSERT_EQUAL_size_t(1, trs_deque_len(d));
    TEST_ASSERT_TRUE(trs_deque_cap(d) >= 1);
    TEST_ASSERT_EQUAL_INT32(42, *TRS_DEQUE_FRONT_AS(int32_t, d));

    trs_deque_drop(d);
}

// a long alternation drives the head all the way round the buffer several times;
// a plain array kept in step is the oracle
static void test_draining_and_refilling_walks_the_ring_round() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_CAP(int32_t, 4, trs_al_default(), &d));

    int32_t want[4] = {0, 0, 0, 0};
    size_t len = 0;

    for (int32_t step = 0; step < 40; ++step) {
        if (len < 4) {
            push_back_int(d, step);
            want[len++] = step;
        }

        trs_deque_pop_front(d);
        for (size_t i = 1; i < len; ++i) {
            want[i - 1] = want[i];
        }
        --len;

        push_back_int(d, step * 10);
        want[len++] = step * 10;

        assert_elems(d, want, len);
    }

    // the capacity never had to grow: the ring reused the slots it already had
    TEST_ASSERT_EQUAL_size_t(4, trs_deque_cap(d));

    trs_deque_drop(d);
}

static void test_get_mut_and_set_write_through_to_the_ring() {
    trs_Deque *d = make_wrapped();

    *TRS_DEQUE_GET_MUT_AS(int32_t, d, 0) = -1;
    TRS_DEQUE_SET(int32_t, d, 3, -4);
    *(int32_t *) trs_deque_front_mut(d) -= 100;
    *(int32_t *) trs_deque_back_mut(d) -= 100;

    assert_elems(d, (int32_t[]){-101, 20, 30, -104}, 4);

    trs_deque_drop(d);
}

/* ========== copy ========== */

static void test_copy_is_independent_of_a_wrapped_source() {
    trs_Deque *d = make_wrapped();

    trs_Deque *copy = nullptr;
    TRS_TEST_OK(trs_deque_copy(d, &copy));

    // the copy is sized to the content, so it comes out in one run
    TEST_ASSERT_FALSE(wraps(copy));
    assert_elems(copy, (int32_t[]){10, 20, 30, 40}, 4);

    TRS_DEQUE_SET(int32_t, copy, 0, 999);
    TEST_ASSERT_EQUAL_INT32(10, *TRS_DEQUE_GET_AS(int32_t, d, 0));

    trs_deque_drop(copy);
    trs_deque_drop(d);
}

static void test_copy_with_builds_on_the_given_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Deque *src = make_wrapped();

    trs_Deque *dst = nullptr;
    TRS_TEST_OK(trs_deque_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_deque_al(dst));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_deque_al(src));
    TEST_ASSERT_TRUE(trs_deque_eq(src, dst));

    // the source is gone and the copy still holds the elems: they were taken, not viewed
    trs_deque_drop(src);
    assert_elems(dst, (int32_t[]){10, 20, 30, 40}, 4);

    trs_deque_drop(dst);
    trs_al_arena_drop(arena);
}

// the blocks are asked of the allocator the copy is going to, not of the source's
static void test_copy_with_reports_an_exhausted_target_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_Deque *src = make_deque(4);

    trs_Deque *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_deque_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(4, trs_deque_len(src));

    trs_deque_drop(src);
    trs_al_arena_drop(arena);
}

static void test_for_each_walks_a_split_ring_in_order() {
    // the contents wrap, so a walk that went by the block instead of by the index would
    // hand back the two runs the wrong way round
    trs_Deque *d = make_wrapped();
    TEST_ASSERT_TRUE(wraps(d));

    int32_t seen[4];
    size_t n = 0;
    TRS_DEQUE_FOR_EACH_AS(int32_t, elem, d) {
        TEST_ASSERT_TRUE(n < 4);
        seen[n++] = *elem;
    }

    TEST_ASSERT_EQUAL_size_t(4, n);
    TEST_ASSERT_EQUAL_INT32_ARRAY(((int32_t[]){10, 20, 30, 40}), seen, 4);

    trs_deque_drop(d);
}

static void test_for_each_over_an_empty_deque_runs_no_body() {
    trs_Deque *d = make_deque(0);

    size_t n = 0;
    TRS_DEQUE_FOR_EACH_AS(int32_t, elem, d) {
        TRS_UNUSED(elem);
        ++n;
    }

    TEST_ASSERT_EQUAL_size_t(0, n);

    trs_deque_drop(d);
}

static void test_for_each_mut_writes_through_every_slot() {
    trs_Deque *d = make_wrapped();

    TRS_DEQUE_FOR_EACH_MUT_AS(int32_t, elem, d) {
        *elem += 1;
    }

    assert_elems(d, (int32_t[]){11, 21, 31, 41}, 4);

    trs_deque_drop(d);
}

static void test_move_assign_hands_over_the_contents_on_one_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Deque *src = nullptr;
    TRS_TEST_OK(TRS_DEQUE_OF(int32_t, &al, &src, 1, 2, 3));

    trs_Deque *dst = nullptr;
    TRS_TEST_OK(TRS_DEQUE_OF(int32_t, &al, &dst, 9));

    const size_t requests = trs_test_probe_requests(&probe);
    TRS_TEST_OK(trs_deque_move_assign(src, dst));

    // nothing was asked of the allocator: the ring changed hands as it stood
    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    assert_elems(dst, (int32_t[]){1, 2, 3}, 3);
    TEST_ASSERT_EQUAL_size_t(0, trs_deque_len(src));

    trs_deque_drop(src);
    trs_deque_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_move_assign_across_allocators_empties_the_source() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    // a split ring must arrive in deque order, exactly as in copy
    trs_Deque *src = make_wrapped();

    trs_Deque *dst = nullptr;
    TRS_TEST_OK(TRS_DEQUE_OF(int32_t, arena, &dst, 9));

    TRS_TEST_OK(trs_deque_move_assign(src, dst));

    assert_elems(dst, (int32_t[]){10, 20, 30, 40}, 4);
    TEST_ASSERT_EQUAL_PTR(arena, trs_deque_al(dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_deque_len(src));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_deque_al(src));

    trs_deque_drop(src);
    trs_deque_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_across_allocators_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Deque *dst = nullptr;
    TRS_TEST_OK(TRS_DEQUE_OF(int32_t, arena, &dst, 9));
    trs_test_arena_leave(arena, 0);

    trs_Deque *src = make_deque(4);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_deque_move_assign(src, dst));

    assert_elems(src, (int32_t[]){0, 1, 2, 3}, 4);
    assert_elems(dst, (int32_t[]){9}, 1);

    trs_deque_drop(src);
    trs_deque_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_of_itself_changes_nothing() {
    trs_Deque *d = make_deque(3);

    TRS_TEST_OK(trs_deque_move_assign(d, d));

    assert_elems(d, (int32_t[]){0, 1, 2}, 3);

    trs_deque_drop(d);
}

static void test_copy_assign_overwrites_a_longer_target() {
    trs_Deque *src = make_deque(2);
    trs_Deque *dst = make_deque(6);

    TRS_TEST_OK(trs_deque_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){0, 1}, 2);

    trs_deque_drop(dst);
    trs_deque_drop(src);
}

static void test_copy_assign_grows_a_shorter_target() {
    trs_Deque *src = make_wrapped();
    trs_Deque *dst = make_deque(1);

    TRS_TEST_OK(trs_deque_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){10, 20, 30, 40}, 4);

    trs_deque_drop(dst);
    trs_deque_drop(src);
}

static void test_copy_assign_of_itself_changes_nothing() {
    trs_Deque *d = make_wrapped();

    TRS_TEST_OK(trs_deque_copy_assign(d, d));

    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);

    trs_deque_drop(d);
}

// the point of the bridge: the split ring arrives in the span as one run, in order
static void test_copy_to_span_unwraps_the_contents() {
    trs_Deque *d = make_wrapped();

    int32_t buf[4] = {0, 0, 0, 0};
    trs_deque_copy_to_span(d, TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 4));

    TEST_ASSERT_EQUAL_INT32_ARRAY(((int32_t[]){10, 20, 30, 40}), buf, 4);

    trs_deque_drop(d);
}

static void test_copy_from_span_writes_back_in_ring_order() {
    trs_Deque *d = make_wrapped();

    constexpr int32_t src[4] = {1, 2, 3, 4};
    trs_deque_copy_from_span(d, TRS_SPAN_FROM_DATA(int32_t, src, 4));

    // the ring is where it was; only the elems changed
    TEST_ASSERT_TRUE(wraps(d));
    assert_elems(d, (int32_t[]){1, 2, 3, 4}, 4);

    trs_deque_drop(d);
}

static void test_the_span_pair_round_trips_an_untouched_deque() {
    trs_Deque *d = make_wrapped();

    int32_t buf[4];
    const trs_SpanMut s = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 4);

    trs_deque_copy_to_span(d, s);
    trs_deque_copy_from_span(d, trs_span_mut_to_span(s));

    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);

    trs_deque_drop(d);
}

// what the pair exists for: hand the contents to algo and take the answer back
static void test_the_span_pair_carries_the_deque_through_algo() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_CAP(int32_t, 4, trs_al_default(), &d));

    push_back_int(d, 1);
    push_back_int(d, 2);
    trs_deque_pop_front(d);
    push_back_int(d, 5);
    push_back_int(d, 3);
    push_back_int(d, 4);
    TEST_ASSERT_TRUE(wraps(d));

    int32_t buf[4];
    const trs_SpanMut s = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, trs_deque_len(d));

    trs_deque_copy_to_span(d, s);
    trs_span_sort(s, trs_cmp_i32);
    trs_deque_copy_from_span(d, trs_span_mut_to_span(s));

    assert_elems(d, (int32_t[]){2, 3, 4, 5}, 4);

    trs_deque_drop(d);
}

/* ========== insert / remove ========== */

static void test_insert_at_the_front_matches_push_front() {
    trs_Deque *d = make_deque(3);

    TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, d, 0, 9));

    assert_elems(d, (int32_t[]){9, 0, 1, 2}, 4);

    trs_deque_drop(d);
}

static void test_insert_at_len_matches_push_back() {
    trs_Deque *d = make_deque(3);

    TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, d, 3, 9));

    assert_elems(d, (int32_t[]){0, 1, 2, 9}, 4);

    trs_deque_drop(d);
}

static void test_insert_into_an_empty_deque() {
    trs_Deque *d = make_deque(0);

    TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, d, 0, 7));

    assert_elems(d, (int32_t[]){7}, 1);

    trs_deque_drop(d);
}

// the two branches shift opposite sides, so both halves need their own case
static void test_insert_in_the_middle_shifts_either_side() {
    trs_Deque *front_half = make_deque(6);
    TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, front_half, 2, 99));
    assert_elems(front_half, (int32_t[]){0, 1, 99, 2, 3, 4, 5}, 7);

    trs_Deque *back_half = make_deque(6);
    TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, back_half, 4, 99));
    assert_elems(back_half, (int32_t[]){0, 1, 2, 3, 99, 4, 5}, 7);

    trs_deque_drop(back_half);
    trs_deque_drop(front_half);
}

// a shift across the seam cannot be one memmove, so the wrapped case is its own test
static void test_insert_into_a_wrapped_ring() {
    trs_Deque *d = make_wrapped();
    trs_deque_pop_back(d); // room for one, still split

    TEST_ASSERT_TRUE(wraps(d));
    TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, d, 1, 15));

    assert_elems(d, (int32_t[]){10, 15, 20, 30}, 4);
    TEST_ASSERT_EQUAL_size_t(4, trs_deque_cap(d));

    trs_deque_drop(d);
}

static void test_insert_moves_whole_elems() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_OF(Pair, trs_al_default(), &d, {1, 10}, {2, 20}, {3, 30}));

    constexpr Pair val = {9, 90};
    TRS_TEST_OK(trs_deque_insert(d, 1, &val));

    constexpr Pair want[4] = {{1, 10}, {9, 90}, {2, 20}, {3, 30}};
    for (size_t i = 0; i < 4; ++i) {
        const Pair *got = TRS_DEQUE_GET_AS(Pair, d, i);
        TEST_ASSERT_EQUAL_INT64(want[i].a, got->a);
        TEST_ASSERT_EQUAL_INT64(want[i].b, got->b);
    }

    trs_deque_drop(d);
}

static void test_remove_at_the_ends_matches_the_pops() {
    trs_Deque *front = make_deque(4);
    trs_deque_remove(front, 0);
    assert_elems(front, (int32_t[]){1, 2, 3}, 3);

    trs_Deque *back = make_deque(4);
    trs_deque_remove(back, 3);
    assert_elems(back, (int32_t[]){0, 1, 2}, 3);

    trs_deque_drop(back);
    trs_deque_drop(front);
}

static void test_remove_in_the_middle_closes_the_gap_from_either_side() {
    trs_Deque *front_half = make_deque(6);
    trs_deque_remove(front_half, 1);
    assert_elems(front_half, (int32_t[]){0, 2, 3, 4, 5}, 5);

    trs_Deque *back_half = make_deque(6);
    trs_deque_remove(back_half, 4);
    assert_elems(back_half, (int32_t[]){0, 1, 2, 3, 5}, 5);

    trs_deque_drop(back_half);
    trs_deque_drop(front_half);
}

static void test_remove_from_a_wrapped_ring() {
    trs_Deque *d = make_wrapped();

    trs_deque_remove(d, 2);

    assert_elems(d, (int32_t[]){10, 20, 40}, 3);

    trs_deque_drop(d);
}

static void test_insert_then_remove_restores_the_deque() {
    trs_Deque *d = make_wrapped();

    for (size_t idx = 0; idx <= 4; ++idx) {
        TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, d, idx, 77));
        TEST_ASSERT_EQUAL_INT32(77, *TRS_DEQUE_GET_AS(int32_t, d, idx));

        trs_deque_remove(d, idx);
        assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);
    }

    trs_deque_drop(d);
}

// the header promises the SHORTER side moves, and which one moved is visible without
// looking inside: the elems on the untouched side keep their addresses. Reserved up
// front so no growth relocates everything and hides the answer
static void test_insert_shifts_the_shorter_side() {
    trs_Deque *front_half = make_deque(8);
    TRS_TEST_OK(trs_deque_reserve(front_half, 16));
    const void *back_elem = trs_deque_back(front_half);

    TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, front_half, 2, 99));
    TEST_ASSERT_EQUAL_PTR(back_elem, trs_deque_back(front_half));

    trs_Deque *back_half = make_deque(8);
    TRS_TEST_OK(trs_deque_reserve(back_half, 16));
    const void *front_elem = trs_deque_front(back_half);

    TRS_TEST_OK(TRS_DEQUE_INSERT(int32_t, back_half, 6, 99));
    TEST_ASSERT_EQUAL_PTR(front_elem, trs_deque_front(back_half));

    trs_deque_drop(back_half);
    trs_deque_drop(front_half);
}

static void test_remove_shifts_the_shorter_side() {
    trs_Deque *front_half = make_deque(8);
    const void *back_elem = trs_deque_back(front_half);

    trs_deque_remove(front_half, 1);
    TEST_ASSERT_EQUAL_PTR(back_elem, trs_deque_back(front_half));

    trs_Deque *back_half = make_deque(8);
    const void *front_elem = trs_deque_front(back_half);

    trs_deque_remove(back_half, 6);
    TEST_ASSERT_EQUAL_PTR(front_elem, trs_deque_front(back_half));

    trs_deque_drop(back_half);
    trs_deque_drop(front_half);
}

/* ========== mods ========== */

static void test_clear_empties_but_keeps_the_capacity() {
    trs_Deque *d = make_wrapped();
    const size_t cap = trs_deque_cap(d);

    trs_deque_clear(d);

    TEST_ASSERT_EQUAL_size_t(0, trs_deque_len(d));
    TEST_ASSERT_EQUAL_size_t(cap, trs_deque_cap(d));

    // and it is usable again from either end
    push_back_int(d, 1);
    push_front_int(d, 0);
    assert_elems(d, (int32_t[]){0, 1}, 2);

    trs_deque_drop(d);
}

static void test_reserve_grows_and_never_shrinks() {
    trs_Deque *d = make_deque(2);

    TRS_TEST_OK(trs_deque_reserve(d, 16));
    TEST_ASSERT_EQUAL_size_t(16, trs_deque_cap(d));

    TRS_TEST_OK(trs_deque_reserve(d, 4));
    TEST_ASSERT_EQUAL_size_t(16, trs_deque_cap(d));

    assert_elems(d, (int32_t[]){0, 1}, 2);

    trs_deque_drop(d);
}

// reserve moves the contents into a fresh block, so the seam disappears
static void test_reserve_unwraps_the_ring() {
    trs_Deque *d = make_wrapped();

    TRS_TEST_OK(trs_deque_reserve(d, 32));

    TEST_ASSERT_FALSE(wraps(d));
    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);

    trs_deque_drop(d);
}

static void test_shrink_to_fit_trims_to_len() {
    trs_Deque *d = make_wrapped();
    trs_deque_pop_back(d);

    TRS_TEST_OK(trs_deque_shrink_to_fit(d));

    TEST_ASSERT_EQUAL_size_t(3, trs_deque_cap(d));
    TEST_ASSERT_FALSE(wraps(d));
    assert_elems(d, (int32_t[]){10, 20, 30}, 3);

    trs_deque_drop(d);
}

static void test_shrink_to_fit_of_an_empty_deque_frees_the_block() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_CAP(int32_t, 8, &al, &d));

    TRS_TEST_OK(trs_deque_shrink_to_fit(d));

    TEST_ASSERT_EQUAL_size_t(0, trs_deque_cap(d));
    TEST_ASSERT_EQUAL_size_t(1, probe.live); // the deque itself, not its buffer

    trs_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_resize_grows_at_the_back_with_zeros() {
    trs_Deque *d = make_wrapped();

    TRS_TEST_OK(trs_deque_resize(d, 6));

    assert_elems(d, (int32_t[]){10, 20, 30, 40, 0, 0}, 6);

    trs_deque_drop(d);
}

// the new tail may straddle the seam, so growing inside the existing capacity is
// a different path from growing past it
static void test_resize_grows_inside_a_wrapped_capacity() {
    trs_Deque *d = make_wrapped();
    trs_deque_pop_back(d);
    trs_deque_pop_back(d);

    TRS_TEST_OK(trs_deque_resize(d, 4));

    TEST_ASSERT_EQUAL_size_t(4, trs_deque_cap(d));
    assert_elems(d, (int32_t[]){10, 20, 0, 0}, 4);

    trs_deque_drop(d);
}

static void test_resize_shrinks_from_the_back() {
    trs_Deque *d = make_wrapped();

    TRS_TEST_OK(trs_deque_resize(d, 2));

    assert_elems(d, (int32_t[]){10, 20}, 2);

    trs_deque_drop(d);
}

static void test_swap_on_one_allocator_hands_over_the_buffers() {
    trs_Deque *a = make_deque(2);
    trs_Deque *b = make_wrapped();

    const size_t a_cap = trs_deque_cap(a);
    const size_t b_cap = trs_deque_cap(b);

    trs_deque_swap(a, b);

    assert_elems(a, (int32_t[]){10, 20, 30, 40}, 4);
    assert_elems(b, (int32_t[]){0, 1}, 2);
    TEST_ASSERT_EQUAL_size_t(b_cap, trs_deque_cap(a));
    TEST_ASSERT_EQUAL_size_t(a_cap, trs_deque_cap(b));

    trs_deque_drop(b);
    trs_deque_drop(a);
}

static void test_swap_of_itself_changes_nothing() {
    trs_Deque *d = make_wrapped();

    trs_deque_swap(d, d);

    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);

    trs_deque_drop(d);
}

static void test_swap_elems_across_the_seam() {
    trs_Deque *d = make_wrapped();

    trs_deque_swap_elems(d, 0, 3);
    assert_elems(d, (int32_t[]){40, 20, 30, 10}, 4);

    // swapping an elem with itself is a no-op, not a self-overwrite
    trs_deque_swap_elems(d, 2, 2);
    assert_elems(d, (int32_t[]){40, 20, 30, 10}, 4);

    trs_deque_drop(d);
}

/* ========== failure ========== */

static void test_new_reports_a_refused_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);
    trs_test_probe_fail_after_next(&probe, 0);

    trs_Deque *d = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_DEQUE_NEW(int32_t, &al, &d));

    TEST_ASSERT_NULL(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the struct is handed out and the buffer refused: nothing may leak
static void test_new_cap_reports_a_refused_buffer() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);
    trs_test_probe_fail_after_next(&probe, 1);

    trs_Deque *d = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_DEQUE_NEW_CAP(int32_t, 4, &al, &d));

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_the_pushes_report_a_refused_growth() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Deque *back = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_CAP(int32_t, 1, &al, &back));
    TRS_TEST_OK(TRS_DEQUE_PUSH_BACK(int32_t, back, 1));

    trs_test_probe_fail_after_next(&probe, 0);
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_DEQUE_PUSH_BACK(int32_t, back, 2));
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_DEQUE_PUSH_FRONT(int32_t, back, 0));

    // the refusal left the deque exactly as it was
    assert_elems(back, (int32_t[]){1}, 1);
    TEST_ASSERT_EQUAL_size_t(1, trs_deque_cap(back));

    trs_deque_drop(back);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_insert_reports_a_refused_growth() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_CAP(int32_t, 2, &al, &d));
    TRS_TEST_OK(TRS_DEQUE_PUSH_BACK(int32_t, d, 1));
    TRS_TEST_OK(TRS_DEQUE_PUSH_BACK(int32_t, d, 2));

    trs_test_probe_fail_after_next(&probe, 0);
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_DEQUE_INSERT(int32_t, d, 1, 99));

    assert_elems(d, (int32_t[]){1, 2}, 2);

    trs_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_reserve_reports_a_refused_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW(int32_t, &al, &d));

    trs_test_probe_fail_after_next(&probe, 0);
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_deque_reserve(d, 8));

    TEST_ASSERT_EQUAL_size_t(0, trs_deque_cap(d));

    trs_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// while the capacity holds, neither end asks the allocator for anything
static void test_the_ends_do_not_allocate_while_the_capacity_holds() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_NEW_CAP(int32_t, 4, &al, &d));

    const size_t before = trs_test_probe_requests(&probe);

    for (int32_t i = 0; i < 100; ++i) {
        TRS_TEST_OK(TRS_DEQUE_PUSH_BACK(int32_t, d, i));
        TRS_TEST_OK(TRS_DEQUE_PUSH_FRONT(int32_t, d, i));
        trs_deque_pop_front(d);
        trs_deque_pop_back(d);
    }

    TEST_ASSERT_EQUAL_size_t(before, trs_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_size_t(4, trs_deque_cap(d));

    trs_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== compare ========== */

static void test_eq_matches_the_same_elems() {
    trs_Deque *a = make_deque(4);
    trs_Deque *b = make_deque(4);

    TEST_ASSERT_TRUE(trs_deque_eq(a, a));
    TEST_ASSERT_TRUE(trs_deque_eq(a, b));
    TEST_ASSERT_TRUE(trs_deque_eq(b, a));
    TEST_ASSERT_TRUE(trs_deque_eq_by(a, b, trs_eq_i32));

    trs_deque_drop(a);
    trs_deque_drop(b);
}

static void test_eq_parts_one_differing_elem() {
    trs_Deque *a = make_deque(4);
    trs_Deque *b = make_deque(4);
    TRS_DEQUE_SET(int32_t, b, 3, 99);

    TEST_ASSERT_FALSE(trs_deque_eq(a, b));
    TEST_ASSERT_FALSE(trs_deque_eq_by(a, b, trs_eq_i32));

    trs_deque_drop(a);
    trs_deque_drop(b);
}

static void test_eq_parts_different_lengths() {
    trs_Deque *a = make_deque(4);
    trs_Deque *shorter = make_deque(3);

    TEST_ASSERT_FALSE(trs_deque_eq(a, shorter));
    TEST_ASSERT_FALSE(trs_deque_eq(shorter, a));

    trs_deque_drop(a);
    trs_deque_drop(shorter);
}

static void test_eq_of_two_empties() {
    trs_Deque *a = make_deque(0);
    trs_Deque *b = make_deque(0);
    trs_Deque *one = make_deque(1);

    TEST_ASSERT_TRUE(trs_deque_eq(a, b));
    TEST_ASSERT_TRUE(trs_deque_eq_by(a, b, trs_eq_i32));
    TEST_ASSERT_FALSE(trs_deque_eq(a, one));

    trs_deque_drop(a);
    trs_deque_drop(one);
    trs_deque_drop(b);
}

// two rings holding the same elems start at different slots, so what is compared is the
// contents in ring order and never the buffers
static void test_eq_ignores_where_the_ring_starts() {
    constexpr int32_t want[4] = {10, 20, 30, 40};

    trs_Deque *straight = nullptr;
    TRS_TEST_OK(TRS_DEQUE_FROM_DATA(int32_t, want, 4, trs_al_default(), &straight));
    trs_Deque *wrapped = make_wrapped();

    TEST_ASSERT_TRUE(wraps(wrapped));
    TEST_ASSERT_FALSE(wraps(straight));
    TEST_ASSERT_TRUE(trs_deque_eq(straight, wrapped));
    TEST_ASSERT_TRUE(trs_deque_eq(wrapped, straight));
    TEST_ASSERT_TRUE(trs_deque_eq_by(wrapped, straight, trs_eq_i32));

    trs_deque_drop(straight);
    trs_deque_drop(wrapped);
}

// the same elems rotated by one: equal as multisets, unequal as deques
static void test_eq_is_order_sensitive() {
    trs_Deque *a = make_deque(4);
    trs_Deque *b = make_deque(4);

    const int32_t front = *TRS_DEQUE_FRONT_AS(int32_t, b);
    trs_deque_pop_front(b);
    push_back_int(b, front);

    TEST_ASSERT_EQUAL_size_t(trs_deque_len(a), trs_deque_len(b));
    TEST_ASSERT_FALSE(trs_deque_eq(a, b));

    trs_deque_drop(a);
    trs_deque_drop(b);
}

static void test_eq_by_asks_the_equality() {
    constexpr Pair lhs[2] = {{1, 10}, {2, 20}};
    constexpr Pair rhs[2] = {{1, 70}, {2, 80}};

    trs_Deque *a = nullptr;
    trs_Deque *b = nullptr;
    TRS_TEST_OK(TRS_DEQUE_FROM_DATA(Pair, lhs, 2, trs_al_default(), &a));
    TRS_TEST_OK(TRS_DEQUE_FROM_DATA(Pair, rhs, 2, trs_al_default(), &b));

    TEST_ASSERT_FALSE(trs_deque_eq(a, b));
    TEST_ASSERT_TRUE(trs_deque_eq_by(a, b, trs_test_pair_eq_a));

    trs_deque_drop(a);
    trs_deque_drop(b);
}

/* ========== print ========== */

// a printer writes to a stream, so a case has to read one back. tmpfile is the portable
// way, the same one test/core/test_print.c takes
static void assert_prints(const char *expected, const trs_Deque *d) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);

    trs_deque_fprint(d, stream, trs_fprint_i32);
    rewind(stream);

    char buf[128];
    const size_t n = fread(buf, 1, sizeof buf - 1, stream);
    buf[n] = '\0';
    fclose(stream);

    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_fprint_writes_the_elems() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_OF(int32_t, trs_al_default(), &d, 5, 3, 1));

    assert_prints("[5, 3, 1]\n", d);

    trs_deque_drop(d);
}

static void test_fprint_of_a_single_elem_has_no_separator() {
    trs_Deque *d = nullptr;
    TRS_TEST_OK(TRS_DEQUE_OF(int32_t, trs_al_default(), &d, 7));

    assert_prints("[7]\n", d);

    trs_deque_drop(d);
}

static void test_fprint_of_an_empty_deque() {
    trs_Deque *d = make_deque(0);

    assert_prints("[]\n", d);

    trs_deque_drop(d);
}

// the stdout twin takes no stream, and C has no portable way to capture one and give it
// back — so a case can only say that it runs and reaches the same printer
static void test_print_writes_to_stdout() {
    trs_Deque *d = make_deque(3);

    trs_deque_print(d, trs_fprint_i32);

    trs_deque_drop(d);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_empty_and_unallocated);
    RUN_TEST(test_new_len_zeroes_its_elems);
    RUN_TEST(test_new_cap_reserves_without_length);
    RUN_TEST(test_of_keeps_the_order);
    RUN_TEST(test_from_span_copies_the_elems);
    RUN_TEST(test_drop_of_null_is_a_no_op);

    RUN_TEST(test_push_back_appends);
    RUN_TEST(test_push_front_prepends);
    RUN_TEST(test_pushes_from_both_ends_meet_in_the_middle);
    RUN_TEST(test_pop_front_and_pop_back_take_from_their_own_ends);
    RUN_TEST(test_first_and_last_follow_the_ends);

    RUN_TEST(test_the_ring_wraps_and_get_stays_relative_to_the_front);
    RUN_TEST(test_growth_while_wrapped_keeps_the_order);
    RUN_TEST(test_growth_while_wrapped_from_the_front_keeps_the_order);
    RUN_TEST(test_push_front_on_an_empty_deque_allocates);
    RUN_TEST(test_draining_and_refilling_walks_the_ring_round);
    RUN_TEST(test_get_mut_and_set_write_through_to_the_ring);

    RUN_TEST(test_copy_is_independent_of_a_wrapped_source);
    RUN_TEST(test_copy_with_builds_on_the_given_allocator);
    RUN_TEST(test_copy_with_reports_an_exhausted_target_arena);
    RUN_TEST(test_for_each_walks_a_split_ring_in_order);
    RUN_TEST(test_for_each_over_an_empty_deque_runs_no_body);
    RUN_TEST(test_for_each_mut_writes_through_every_slot);

    RUN_TEST(test_move_assign_hands_over_the_contents_on_one_allocator);
    RUN_TEST(test_move_assign_across_allocators_empties_the_source);
    RUN_TEST(test_move_assign_across_allocators_reports_an_exhausted_arena);
    RUN_TEST(test_move_assign_of_itself_changes_nothing);
    RUN_TEST(test_copy_assign_overwrites_a_longer_target);
    RUN_TEST(test_copy_assign_grows_a_shorter_target);
    RUN_TEST(test_copy_assign_of_itself_changes_nothing);
    RUN_TEST(test_copy_to_span_unwraps_the_contents);
    RUN_TEST(test_copy_from_span_writes_back_in_ring_order);
    RUN_TEST(test_the_span_pair_round_trips_an_untouched_deque);
    RUN_TEST(test_the_span_pair_carries_the_deque_through_algo);

    RUN_TEST(test_insert_at_the_front_matches_push_front);
    RUN_TEST(test_insert_at_len_matches_push_back);
    RUN_TEST(test_insert_into_an_empty_deque);
    RUN_TEST(test_insert_in_the_middle_shifts_either_side);
    RUN_TEST(test_insert_into_a_wrapped_ring);
    RUN_TEST(test_insert_moves_whole_elems);
    RUN_TEST(test_remove_at_the_ends_matches_the_pops);
    RUN_TEST(test_remove_in_the_middle_closes_the_gap_from_either_side);
    RUN_TEST(test_remove_from_a_wrapped_ring);
    RUN_TEST(test_insert_then_remove_restores_the_deque);
    RUN_TEST(test_insert_shifts_the_shorter_side);
    RUN_TEST(test_remove_shifts_the_shorter_side);

    RUN_TEST(test_clear_empties_but_keeps_the_capacity);
    RUN_TEST(test_reserve_grows_and_never_shrinks);
    RUN_TEST(test_reserve_unwraps_the_ring);
    RUN_TEST(test_shrink_to_fit_trims_to_len);
    RUN_TEST(test_shrink_to_fit_of_an_empty_deque_frees_the_block);
    RUN_TEST(test_resize_grows_at_the_back_with_zeros);
    RUN_TEST(test_resize_grows_inside_a_wrapped_capacity);
    RUN_TEST(test_resize_shrinks_from_the_back);
    RUN_TEST(test_swap_on_one_allocator_hands_over_the_buffers);
    RUN_TEST(test_swap_of_itself_changes_nothing);
    RUN_TEST(test_swap_elems_across_the_seam);

    RUN_TEST(test_new_reports_a_refused_allocator);
    RUN_TEST(test_new_cap_reports_a_refused_buffer);
    RUN_TEST(test_the_pushes_report_a_refused_growth);
    RUN_TEST(test_insert_reports_a_refused_growth);
    RUN_TEST(test_reserve_reports_a_refused_allocator);
    RUN_TEST(test_the_ends_do_not_allocate_while_the_capacity_holds);


    RUN_TEST(test_eq_matches_the_same_elems);
    RUN_TEST(test_eq_parts_one_differing_elem);
    RUN_TEST(test_eq_parts_different_lengths);
    RUN_TEST(test_eq_of_two_empties);
    RUN_TEST(test_eq_ignores_where_the_ring_starts);
    RUN_TEST(test_eq_is_order_sensitive);
    RUN_TEST(test_eq_by_asks_the_equality);

    RUN_TEST(test_fprint_writes_the_elems);
    RUN_TEST(test_fprint_of_a_single_elem_has_no_separator);
    RUN_TEST(test_fprint_of_an_empty_deque);
    RUN_TEST(test_print_writes_to_stdout);

    return UNITY_END();
}
