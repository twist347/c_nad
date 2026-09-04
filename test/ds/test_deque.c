#include "nad/ds/deque.h"
#include "nad/algo/sort.h"
#include "nad/alloc/arena.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"

#include "support/arena.h"
#include "support/pair.h"
#include "support/probe.h"
#include "support/status.h"

#include "unity.h"

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
static bool wraps(const nad_Deque *d) {
    return (const unsigned char *) nad_deque_first(d) > (const unsigned char *) nad_deque_last(d);
}

static void assert_elems(const nad_Deque *d, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, nad_deque_len(d));

    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_INT32(want[i], *NAD_DEQUE_GET_AS(int32_t, d, i));
    }
}

static void push_back_int(nad_Deque *d, int32_t val) {
    NAD_TEST_OK(nad_deque_push_back(d, &val));
}

static void push_front_int(nad_Deque *d, int32_t val) {
    NAD_TEST_OK(nad_deque_push_front(d, &val));
}

// int32_t deque holding 0, 1, ... len-1, filled from the back
[[nodiscard]]
static nad_Deque *make_deque(size_t len) {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW(int32_t, nad_al_default(), &d));

    for (size_t i = 0; i < len; ++i) {
        push_back_int(d, (int32_t) i);
    }
    return d;
}

// {10, 20, 30, 40} in a ring of exactly four slots that is guaranteed to be split:
// two elems are pushed off the front and the same number wrapped around onto the back
[[nodiscard]]
static nad_Deque *make_wrapped(void) {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_CAP(int32_t, 4, nad_al_default(), &d));

    push_back_int(d, 1);
    push_back_int(d, 2);
    push_back_int(d, 10);
    push_back_int(d, 20);

    nad_deque_pop_front(d);
    nad_deque_pop_front(d);

    push_back_int(d, 30);
    push_back_int(d, 40);

    TEST_ASSERT_EQUAL_size_t(4, nad_deque_cap(d));
    TEST_ASSERT_TRUE(wraps(d));

    return d;
}

/* ========== lifetime ========== */

static void test_new_starts_empty_and_unallocated() {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW(int32_t, nad_al_default(), &d));

    TEST_ASSERT_EQUAL_size_t(0, nad_deque_len(d));
    TEST_ASSERT_EQUAL_size_t(0, nad_deque_cap(d));
    TEST_ASSERT_EQUAL_size_t(0, nad_deque_bytes(d));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_deque_elem_size(d));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_deque_al(d));

    nad_deque_drop(d);
}

static void test_new_len_zeroes_its_elems() {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_LEN(int32_t, 3, nad_al_default(), &d));

    assert_elems(d, (int32_t[]){0, 0, 0}, 3);
    TEST_ASSERT_EQUAL_size_t(3 * sizeof(int32_t), nad_deque_bytes(d));

    nad_deque_drop(d);
}

static void test_new_cap_reserves_without_length() {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_CAP(int32_t, 8, nad_al_default(), &d));

    TEST_ASSERT_EQUAL_size_t(0, nad_deque_len(d));
    TEST_ASSERT_EQUAL_size_t(8, nad_deque_cap(d));

    nad_deque_drop(d);
}

static void test_of_keeps_the_order() {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_OF(int32_t, nad_al_default(), &d, 5, 6, 7));

    assert_elems(d, (int32_t[]){5, 6, 7}, 3);
    TEST_ASSERT_FALSE(wraps(d));

    nad_deque_drop(d);
}

static void test_from_span_copies_the_elems() {
    constexpr int32_t src[4] = {9, 8, 7, 6};

    nad_Deque *d = nullptr;
    NAD_TEST_OK(nad_deque_from_span(NAD_SPAN_NEW(int32_t, src, 4), nad_al_default(), &d));

    assert_elems(d, (int32_t[]){9, 8, 7, 6}, 4);

    nad_deque_drop(d);
}

static void test_drop_of_null_is_a_no_op() {
    nad_deque_drop(nullptr);
}

/* ========== ends ========== */

static void test_push_back_appends() {
    nad_Deque *d = make_deque(0);

    push_back_int(d, 1);
    push_back_int(d, 2);
    push_back_int(d, 3);

    assert_elems(d, (int32_t[]){1, 2, 3}, 3);

    nad_deque_drop(d);
}

// the same calls from the other end come out reversed — that is the whole difference
static void test_push_front_prepends() {
    nad_Deque *d = make_deque(0);

    push_front_int(d, 1);
    push_front_int(d, 2);
    push_front_int(d, 3);

    assert_elems(d, (int32_t[]){3, 2, 1}, 3);

    nad_deque_drop(d);
}

static void test_pushes_from_both_ends_meet_in_the_middle() {
    nad_Deque *d = make_deque(0);

    push_back_int(d, 0);
    push_front_int(d, -1);
    push_back_int(d, 1);
    push_front_int(d, -2);
    push_back_int(d, 2);

    assert_elems(d, (int32_t[]){-2, -1, 0, 1, 2}, 5);

    nad_deque_drop(d);
}

static void test_pop_front_and_pop_back_take_from_their_own_ends() {
    nad_Deque *d = make_deque(5);

    nad_deque_pop_front(d);
    nad_deque_pop_back(d);

    assert_elems(d, (int32_t[]){1, 2, 3}, 3);

    nad_deque_drop(d);
}

static void test_first_and_last_follow_the_ends() {
    nad_Deque *d = make_deque(3);

    TEST_ASSERT_EQUAL_INT32(0, *NAD_DEQUE_FIRST_AS(int32_t, d));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_DEQUE_LAST_AS(int32_t, d));

    push_front_int(d, 9);
    push_back_int(d, 8);

    TEST_ASSERT_EQUAL_INT32(9, *NAD_DEQUE_FIRST_AS(int32_t, d));
    TEST_ASSERT_EQUAL_INT32(8, *NAD_DEQUE_LAST_AS(int32_t, d));

    // a single elem is both ends at once
    nad_Deque *one = make_deque(1);
    TEST_ASSERT_EQUAL_PTR(nad_deque_first(one), nad_deque_last(one));

    nad_deque_drop(one);
    nad_deque_drop(d);
}

/* ========== the ring ========== */

static void test_the_ring_wraps_and_get_stays_relative_to_the_front() {
    nad_Deque *d = make_wrapped();

    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);
    TEST_ASSERT_EQUAL_INT32(10, *NAD_DEQUE_FIRST_AS(int32_t, d));
    TEST_ASSERT_EQUAL_INT32(40, *NAD_DEQUE_LAST_AS(int32_t, d));

    nad_deque_drop(d);
}

// the classic ring bug: growing a split ring must unroll it, not copy the block
static void test_growth_while_wrapped_keeps_the_order() {
    nad_Deque *d = make_wrapped();

    push_back_int(d, 50);

    TEST_ASSERT_TRUE(nad_deque_cap(d) > 4);
    TEST_ASSERT_FALSE(wraps(d));
    assert_elems(d, (int32_t[]){10, 20, 30, 40, 50}, 5);

    nad_deque_drop(d);
}

// the same growth from the other end: the new front must not land inside the old run
static void test_growth_while_wrapped_from_the_front_keeps_the_order() {
    nad_Deque *d = make_wrapped();

    push_front_int(d, 5);

    TEST_ASSERT_TRUE(nad_deque_cap(d) > 4);
    assert_elems(d, (int32_t[]){5, 10, 20, 30, 40}, 5);

    nad_deque_drop(d);
}

static void test_push_front_on_an_empty_deque_allocates() {
    nad_Deque *d = make_deque(0);

    push_front_int(d, 42);

    TEST_ASSERT_EQUAL_size_t(1, nad_deque_len(d));
    TEST_ASSERT_TRUE(nad_deque_cap(d) >= 1);
    TEST_ASSERT_EQUAL_INT32(42, *NAD_DEQUE_FIRST_AS(int32_t, d));

    nad_deque_drop(d);
}

// a long alternation drives the head all the way round the buffer several times;
// a plain array kept in step is the oracle
static void test_draining_and_refilling_walks_the_ring_round() {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_CAP(int32_t, 4, nad_al_default(), &d));

    int32_t want[4] = {0, 0, 0, 0};
    size_t len = 0;

    for (int32_t step = 0; step < 40; ++step) {
        if (len < 4) {
            push_back_int(d, step);
            want[len++] = step;
        }

        nad_deque_pop_front(d);
        for (size_t i = 1; i < len; ++i) {
            want[i - 1] = want[i];
        }
        --len;

        push_back_int(d, step * 10);
        want[len++] = step * 10;

        assert_elems(d, want, len);
    }

    // the capacity never had to grow: the ring reused the slots it already had
    TEST_ASSERT_EQUAL_size_t(4, nad_deque_cap(d));

    nad_deque_drop(d);
}

static void test_get_mut_and_set_write_through_to_the_ring() {
    nad_Deque *d = make_wrapped();

    *NAD_DEQUE_GET_MUT_AS(int32_t, d, 0) = -1;
    NAD_DEQUE_SET(int32_t, d, 3, -4);
    *(int32_t *) nad_deque_first_mut(d) -= 100;
    *(int32_t *) nad_deque_last_mut(d) -= 100;

    assert_elems(d, (int32_t[]){-101, 20, 30, -104}, 4);

    nad_deque_drop(d);
}

/* ========== copy ========== */

static void test_copy_is_independent_of_a_wrapped_source() {
    nad_Deque *d = make_wrapped();

    nad_Deque *copy = nullptr;
    NAD_TEST_OK(nad_deque_copy(d, &copy));

    // the copy is sized to the content, so it comes out in one run
    TEST_ASSERT_FALSE(wraps(copy));
    assert_elems(copy, (int32_t[]){10, 20, 30, 40}, 4);

    NAD_DEQUE_SET(int32_t, copy, 0, 999);
    TEST_ASSERT_EQUAL_INT32(10, *NAD_DEQUE_GET_AS(int32_t, d, 0));

    nad_deque_drop(copy);
    nad_deque_drop(d);
}

static void test_copy_with_builds_on_the_given_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Deque *src = make_wrapped();

    nad_Deque *dst = nullptr;
    NAD_TEST_OK(nad_deque_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, nad_deque_al(dst));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_deque_al(src));
    TEST_ASSERT_TRUE(nad_deque_eq(src, dst));

    // the source is gone and the copy still holds the elems: they were taken, not viewed
    nad_deque_drop(src);
    assert_elems(dst, (int32_t[]){10, 20, 30, 40}, 4);

    nad_deque_drop(dst);
    nad_al_arena_drop(arena);
}

// the blocks are asked of the allocator the copy is going to, not of the source's
static void test_copy_with_reports_an_exhausted_target_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);
    nad_test_arena_leave(arena, 0);

    nad_Deque *src = make_deque(4);

    nad_Deque *dst = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_deque_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(4, nad_deque_len(src));

    nad_deque_drop(src);
    nad_al_arena_drop(arena);
}

static void test_move_assign_hands_over_the_contents_on_one_allocator() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Deque *src = nullptr;
    NAD_TEST_OK(NAD_DEQUE_OF(int32_t, &al, &src, 1, 2, 3));

    nad_Deque *dst = nullptr;
    NAD_TEST_OK(NAD_DEQUE_OF(int32_t, &al, &dst, 9));

    const size_t requests = nad_test_probe_requests(&probe);
    NAD_TEST_OK(nad_deque_move_assign(src, dst));

    // nothing was asked of the allocator: the ring changed hands as it stood
    TEST_ASSERT_EQUAL_size_t(requests, nad_test_probe_requests(&probe));

    assert_elems(dst, (int32_t[]){1, 2, 3}, 3);
    TEST_ASSERT_EQUAL_size_t(0, nad_deque_len(src));

    nad_deque_drop(src);
    nad_deque_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_move_assign_across_allocators_empties_the_source() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    // a split ring must arrive in deque order, exactly as in copy
    nad_Deque *src = make_wrapped();

    nad_Deque *dst = nullptr;
    NAD_TEST_OK(NAD_DEQUE_OF(int32_t, arena, &dst, 9));

    NAD_TEST_OK(nad_deque_move_assign(src, dst));

    assert_elems(dst, (int32_t[]){10, 20, 30, 40}, 4);
    TEST_ASSERT_EQUAL_PTR(arena, nad_deque_al(dst));

    TEST_ASSERT_EQUAL_size_t(0, nad_deque_len(src));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_deque_al(src));

    nad_deque_drop(src);
    nad_deque_drop(dst);
    nad_al_arena_drop(arena);
}

static void test_move_assign_across_allocators_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Deque *dst = nullptr;
    NAD_TEST_OK(NAD_DEQUE_OF(int32_t, arena, &dst, 9));
    nad_test_arena_leave(arena, 0);

    nad_Deque *src = make_deque(4);

    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_deque_move_assign(src, dst));

    assert_elems(src, (int32_t[]){0, 1, 2, 3}, 4);
    assert_elems(dst, (int32_t[]){9}, 1);

    nad_deque_drop(src);
    nad_deque_drop(dst);
    nad_al_arena_drop(arena);
}

static void test_move_assign_of_itself_changes_nothing() {
    nad_Deque *d = make_deque(3);

    NAD_TEST_OK(nad_deque_move_assign(d, d));

    assert_elems(d, (int32_t[]){0, 1, 2}, 3);

    nad_deque_drop(d);
}

static void test_copy_assign_overwrites_a_longer_target() {
    nad_Deque *src = make_deque(2);
    nad_Deque *dst = make_deque(6);

    NAD_TEST_OK(nad_deque_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){0, 1}, 2);

    nad_deque_drop(dst);
    nad_deque_drop(src);
}

static void test_copy_assign_grows_a_shorter_target() {
    nad_Deque *src = make_wrapped();
    nad_Deque *dst = make_deque(1);

    NAD_TEST_OK(nad_deque_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){10, 20, 30, 40}, 4);

    nad_deque_drop(dst);
    nad_deque_drop(src);
}

static void test_copy_assign_of_itself_changes_nothing() {
    nad_Deque *d = make_wrapped();

    NAD_TEST_OK(nad_deque_copy_assign(d, d));

    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);

    nad_deque_drop(d);
}

// the point of the bridge: the split ring arrives in the span as one run, in order
static void test_copy_to_span_unwraps_the_contents() {
    nad_Deque *d = make_wrapped();

    int32_t buf[4] = {0, 0, 0, 0};
    nad_deque_copy_to_span(d, NAD_SPAN_NEW_MUT(int32_t, buf, 4));

    TEST_ASSERT_EQUAL_INT32_ARRAY(((int32_t[]){10, 20, 30, 40}), buf, 4);

    nad_deque_drop(d);
}

static void test_copy_from_span_writes_back_in_ring_order() {
    nad_Deque *d = make_wrapped();

    constexpr int32_t src[4] = {1, 2, 3, 4};
    nad_deque_copy_from_span(d, NAD_SPAN_NEW(int32_t, src, 4));

    // the ring is where it was; only the elems changed
    TEST_ASSERT_TRUE(wraps(d));
    assert_elems(d, (int32_t[]){1, 2, 3, 4}, 4);

    nad_deque_drop(d);
}

static void test_the_span_pair_round_trips_an_untouched_deque() {
    nad_Deque *d = make_wrapped();

    int32_t buf[4];
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, 4);

    nad_deque_copy_to_span(d, s);
    nad_deque_copy_from_span(d, nad_span_mut_to_span(s));

    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);

    nad_deque_drop(d);
}

// what the pair exists for: hand the contents to algo and take the answer back
static void test_the_span_pair_carries_the_deque_through_algo() {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_CAP(int32_t, 4, nad_al_default(), &d));

    push_back_int(d, 1);
    push_back_int(d, 2);
    nad_deque_pop_front(d);
    push_back_int(d, 5);
    push_back_int(d, 3);
    push_back_int(d, 4);
    TEST_ASSERT_TRUE(wraps(d));

    int32_t buf[4];
    const nad_SpanMut s = NAD_SPAN_NEW_MUT(int32_t, buf, nad_deque_len(d));

    nad_deque_copy_to_span(d, s);
    nad_span_sort(s, nad_cmp_i32);
    nad_deque_copy_from_span(d, nad_span_mut_to_span(s));

    assert_elems(d, (int32_t[]){2, 3, 4, 5}, 4);

    nad_deque_drop(d);
}

/* ========== insert / remove ========== */

static void test_insert_at_the_front_matches_push_front() {
    nad_Deque *d = make_deque(3);

    NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, d, 0, 9));

    assert_elems(d, (int32_t[]){9, 0, 1, 2}, 4);

    nad_deque_drop(d);
}

static void test_insert_at_len_matches_push_back() {
    nad_Deque *d = make_deque(3);

    NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, d, 3, 9));

    assert_elems(d, (int32_t[]){0, 1, 2, 9}, 4);

    nad_deque_drop(d);
}

static void test_insert_into_an_empty_deque() {
    nad_Deque *d = make_deque(0);

    NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, d, 0, 7));

    assert_elems(d, (int32_t[]){7}, 1);

    nad_deque_drop(d);
}

// the two branches shift opposite sides, so both halves need their own case
static void test_insert_in_the_middle_shifts_either_side() {
    nad_Deque *front_half = make_deque(6);
    NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, front_half, 2, 99));
    assert_elems(front_half, (int32_t[]){0, 1, 99, 2, 3, 4, 5}, 7);

    nad_Deque *back_half = make_deque(6);
    NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, back_half, 4, 99));
    assert_elems(back_half, (int32_t[]){0, 1, 2, 3, 99, 4, 5}, 7);

    nad_deque_drop(back_half);
    nad_deque_drop(front_half);
}

// a shift across the seam cannot be one memmove, so the wrapped case is its own test
static void test_insert_into_a_wrapped_ring() {
    nad_Deque *d = make_wrapped();
    nad_deque_pop_back(d); // room for one, still split

    TEST_ASSERT_TRUE(wraps(d));
    NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, d, 1, 15));

    assert_elems(d, (int32_t[]){10, 15, 20, 30}, 4);
    TEST_ASSERT_EQUAL_size_t(4, nad_deque_cap(d));

    nad_deque_drop(d);
}

static void test_insert_moves_whole_elems() {
    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_OF(Pair, nad_al_default(), &d, {1, 10}, {2, 20}, {3, 30}));

    constexpr Pair val = {9, 90};
    NAD_TEST_OK(nad_deque_insert(d, 1, &val));

    constexpr Pair want[4] = {{1, 10}, {9, 90}, {2, 20}, {3, 30}};
    for (size_t i = 0; i < 4; ++i) {
        const Pair *got = NAD_DEQUE_GET_AS(Pair, d, i);
        TEST_ASSERT_EQUAL_INT64(want[i].a, got->a);
        TEST_ASSERT_EQUAL_INT64(want[i].b, got->b);
    }

    nad_deque_drop(d);
}

static void test_remove_at_the_ends_matches_the_pops() {
    nad_Deque *front = make_deque(4);
    nad_deque_remove(front, 0);
    assert_elems(front, (int32_t[]){1, 2, 3}, 3);

    nad_Deque *back = make_deque(4);
    nad_deque_remove(back, 3);
    assert_elems(back, (int32_t[]){0, 1, 2}, 3);

    nad_deque_drop(back);
    nad_deque_drop(front);
}

static void test_remove_in_the_middle_closes_the_gap_from_either_side() {
    nad_Deque *front_half = make_deque(6);
    nad_deque_remove(front_half, 1);
    assert_elems(front_half, (int32_t[]){0, 2, 3, 4, 5}, 5);

    nad_Deque *back_half = make_deque(6);
    nad_deque_remove(back_half, 4);
    assert_elems(back_half, (int32_t[]){0, 1, 2, 3, 5}, 5);

    nad_deque_drop(back_half);
    nad_deque_drop(front_half);
}

static void test_remove_from_a_wrapped_ring() {
    nad_Deque *d = make_wrapped();

    nad_deque_remove(d, 2);

    assert_elems(d, (int32_t[]){10, 20, 40}, 3);

    nad_deque_drop(d);
}

static void test_insert_then_remove_restores_the_deque() {
    nad_Deque *d = make_wrapped();

    for (size_t idx = 0; idx <= 4; ++idx) {
        NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, d, idx, 77));
        TEST_ASSERT_EQUAL_INT32(77, *NAD_DEQUE_GET_AS(int32_t, d, idx));

        nad_deque_remove(d, idx);
        assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);
    }

    nad_deque_drop(d);
}

// the header promises the SHORTER side moves, and which one moved is visible without
// looking inside: the elems on the untouched side keep their addresses. Reserved up
// front so no growth relocates everything and hides the answer
static void test_insert_shifts_the_shorter_side() {
    nad_Deque *front_half = make_deque(8);
    NAD_TEST_OK(nad_deque_reserve(front_half, 16));
    const void *back_elem = nad_deque_last(front_half);

    NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, front_half, 2, 99));
    TEST_ASSERT_EQUAL_PTR(back_elem, nad_deque_last(front_half));

    nad_Deque *back_half = make_deque(8);
    NAD_TEST_OK(nad_deque_reserve(back_half, 16));
    const void *front_elem = nad_deque_first(back_half);

    NAD_TEST_OK(NAD_DEQUE_INSERT(int32_t, back_half, 6, 99));
    TEST_ASSERT_EQUAL_PTR(front_elem, nad_deque_first(back_half));

    nad_deque_drop(back_half);
    nad_deque_drop(front_half);
}

static void test_remove_shifts_the_shorter_side() {
    nad_Deque *front_half = make_deque(8);
    const void *back_elem = nad_deque_last(front_half);

    nad_deque_remove(front_half, 1);
    TEST_ASSERT_EQUAL_PTR(back_elem, nad_deque_last(front_half));

    nad_Deque *back_half = make_deque(8);
    const void *front_elem = nad_deque_first(back_half);

    nad_deque_remove(back_half, 6);
    TEST_ASSERT_EQUAL_PTR(front_elem, nad_deque_first(back_half));

    nad_deque_drop(back_half);
    nad_deque_drop(front_half);
}

/* ========== mods ========== */

static void test_clear_empties_but_keeps_the_capacity() {
    nad_Deque *d = make_wrapped();
    const size_t cap = nad_deque_cap(d);

    nad_deque_clear(d);

    TEST_ASSERT_EQUAL_size_t(0, nad_deque_len(d));
    TEST_ASSERT_EQUAL_size_t(cap, nad_deque_cap(d));

    // and it is usable again from either end
    push_back_int(d, 1);
    push_front_int(d, 0);
    assert_elems(d, (int32_t[]){0, 1}, 2);

    nad_deque_drop(d);
}

static void test_reserve_grows_and_never_shrinks() {
    nad_Deque *d = make_deque(2);

    NAD_TEST_OK(nad_deque_reserve(d, 16));
    TEST_ASSERT_EQUAL_size_t(16, nad_deque_cap(d));

    NAD_TEST_OK(nad_deque_reserve(d, 4));
    TEST_ASSERT_EQUAL_size_t(16, nad_deque_cap(d));

    assert_elems(d, (int32_t[]){0, 1}, 2);

    nad_deque_drop(d);
}

// reserve moves the contents into a fresh block, so the seam disappears
static void test_reserve_unwraps_the_ring() {
    nad_Deque *d = make_wrapped();

    NAD_TEST_OK(nad_deque_reserve(d, 32));

    TEST_ASSERT_FALSE(wraps(d));
    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);

    nad_deque_drop(d);
}

static void test_shrink_to_fit_trims_to_len() {
    nad_Deque *d = make_wrapped();
    nad_deque_pop_back(d);

    NAD_TEST_OK(nad_deque_shrink_to_fit(d));

    TEST_ASSERT_EQUAL_size_t(3, nad_deque_cap(d));
    TEST_ASSERT_FALSE(wraps(d));
    assert_elems(d, (int32_t[]){10, 20, 30}, 3);

    nad_deque_drop(d);
}

static void test_shrink_to_fit_of_an_empty_deque_frees_the_block() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_CAP(int32_t, 8, &al, &d));

    NAD_TEST_OK(nad_deque_shrink_to_fit(d));

    TEST_ASSERT_EQUAL_size_t(0, nad_deque_cap(d));
    TEST_ASSERT_EQUAL_size_t(1, probe.live); // the deque itself, not its buffer

    nad_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_resize_grows_at_the_back_with_zeros() {
    nad_Deque *d = make_wrapped();

    NAD_TEST_OK(nad_deque_resize(d, 6));

    assert_elems(d, (int32_t[]){10, 20, 30, 40, 0, 0}, 6);

    nad_deque_drop(d);
}

// the new tail may straddle the seam, so growing inside the existing capacity is
// a different path from growing past it
static void test_resize_grows_inside_a_wrapped_capacity() {
    nad_Deque *d = make_wrapped();
    nad_deque_pop_back(d);
    nad_deque_pop_back(d);

    NAD_TEST_OK(nad_deque_resize(d, 4));

    TEST_ASSERT_EQUAL_size_t(4, nad_deque_cap(d));
    assert_elems(d, (int32_t[]){10, 20, 0, 0}, 4);

    nad_deque_drop(d);
}

static void test_resize_shrinks_from_the_back() {
    nad_Deque *d = make_wrapped();

    NAD_TEST_OK(nad_deque_resize(d, 2));

    assert_elems(d, (int32_t[]){10, 20}, 2);

    nad_deque_drop(d);
}

static void test_swap_on_one_allocator_hands_over_the_buffers() {
    nad_Deque *a = make_deque(2);
    nad_Deque *b = make_wrapped();

    const size_t a_cap = nad_deque_cap(a);
    const size_t b_cap = nad_deque_cap(b);

    nad_deque_swap(a, b);

    assert_elems(a, (int32_t[]){10, 20, 30, 40}, 4);
    assert_elems(b, (int32_t[]){0, 1}, 2);
    TEST_ASSERT_EQUAL_size_t(b_cap, nad_deque_cap(a));
    TEST_ASSERT_EQUAL_size_t(a_cap, nad_deque_cap(b));

    nad_deque_drop(b);
    nad_deque_drop(a);
}

static void test_swap_of_itself_changes_nothing() {
    nad_Deque *d = make_wrapped();

    nad_deque_swap(d, d);

    assert_elems(d, (int32_t[]){10, 20, 30, 40}, 4);

    nad_deque_drop(d);
}

static void test_swap_elems_across_the_seam() {
    nad_Deque *d = make_wrapped();

    nad_deque_swap_elems(d, 0, 3);
    assert_elems(d, (int32_t[]){40, 20, 30, 10}, 4);

    // swapping an elem with itself is a no-op, not a self-overwrite
    nad_deque_swap_elems(d, 2, 2);
    assert_elems(d, (int32_t[]){40, 20, 30, 10}, 4);

    nad_deque_drop(d);
}

/* ========== failure ========== */

static void test_new_reports_a_refused_allocator() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);
    nad_test_probe_fail_after_next(&probe, 0);

    nad_Deque *d = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, NAD_DEQUE_NEW(int32_t, &al, &d));

    TEST_ASSERT_NULL(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the struct is handed out and the buffer refused: nothing may leak
static void test_new_cap_reports_a_refused_buffer() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);
    nad_test_probe_fail_after_next(&probe, 1);

    nad_Deque *d = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, NAD_DEQUE_NEW_CAP(int32_t, 4, &al, &d));

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_the_pushes_report_a_refused_growth() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Deque *back = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_CAP(int32_t, 1, &al, &back));
    NAD_TEST_OK(NAD_DEQUE_PUSH_BACK(int32_t, back, 1));

    nad_test_probe_fail_after_next(&probe, 0);
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, NAD_DEQUE_PUSH_BACK(int32_t, back, 2));
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, NAD_DEQUE_PUSH_FRONT(int32_t, back, 0));

    // the refusal left the deque exactly as it was
    assert_elems(back, (int32_t[]){1}, 1);
    TEST_ASSERT_EQUAL_size_t(1, nad_deque_cap(back));

    nad_deque_drop(back);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_insert_reports_a_refused_growth() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_CAP(int32_t, 2, &al, &d));
    NAD_TEST_OK(NAD_DEQUE_PUSH_BACK(int32_t, d, 1));
    NAD_TEST_OK(NAD_DEQUE_PUSH_BACK(int32_t, d, 2));

    nad_test_probe_fail_after_next(&probe, 0);
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, NAD_DEQUE_INSERT(int32_t, d, 1, 99));

    assert_elems(d, (int32_t[]){1, 2}, 2);

    nad_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_reserve_reports_a_refused_allocator() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW(int32_t, &al, &d));

    nad_test_probe_fail_after_next(&probe, 0);
    NAD_TEST_STATUS(NAD_STATUS_ERR_NO_MEM, nad_deque_reserve(d, 8));

    TEST_ASSERT_EQUAL_size_t(0, nad_deque_cap(d));

    nad_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// while the capacity holds, neither end asks the allocator for anything
static void test_the_ends_do_not_allocate_while_the_capacity_holds() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Deque *d = nullptr;
    NAD_TEST_OK(NAD_DEQUE_NEW_CAP(int32_t, 4, &al, &d));

    const size_t before = nad_test_probe_requests(&probe);

    for (int32_t i = 0; i < 100; ++i) {
        NAD_TEST_OK(NAD_DEQUE_PUSH_BACK(int32_t, d, i));
        NAD_TEST_OK(NAD_DEQUE_PUSH_FRONT(int32_t, d, i));
        nad_deque_pop_front(d);
        nad_deque_pop_back(d);
    }

    TEST_ASSERT_EQUAL_size_t(before, nad_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_size_t(4, nad_deque_cap(d));

    nad_deque_drop(d);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== compare ========== */

static void test_eq_matches_the_same_elems() {
    nad_Deque *a = make_deque(4);
    nad_Deque *b = make_deque(4);

    TEST_ASSERT_TRUE(nad_deque_eq(a, a));
    TEST_ASSERT_TRUE(nad_deque_eq(a, b));
    TEST_ASSERT_TRUE(nad_deque_eq(b, a));
    TEST_ASSERT_TRUE(nad_deque_eq_by(a, b, nad_eq_i32));

    nad_deque_drop(a);
    nad_deque_drop(b);
}

static void test_eq_parts_one_differing_elem() {
    nad_Deque *a = make_deque(4);
    nad_Deque *b = make_deque(4);
    NAD_DEQUE_SET(int32_t, b, 3, 99);

    TEST_ASSERT_FALSE(nad_deque_eq(a, b));
    TEST_ASSERT_FALSE(nad_deque_eq_by(a, b, nad_eq_i32));

    nad_deque_drop(a);
    nad_deque_drop(b);
}

static void test_eq_parts_different_lengths() {
    nad_Deque *a = make_deque(4);
    nad_Deque *shorter = make_deque(3);

    TEST_ASSERT_FALSE(nad_deque_eq(a, shorter));
    TEST_ASSERT_FALSE(nad_deque_eq(shorter, a));

    nad_deque_drop(a);
    nad_deque_drop(shorter);
}

static void test_eq_of_two_empties() {
    nad_Deque *a = make_deque(0);
    nad_Deque *b = make_deque(0);
    nad_Deque *one = make_deque(1);

    TEST_ASSERT_TRUE(nad_deque_eq(a, b));
    TEST_ASSERT_TRUE(nad_deque_eq_by(a, b, nad_eq_i32));
    TEST_ASSERT_FALSE(nad_deque_eq(a, one));

    nad_deque_drop(a);
    nad_deque_drop(one);
    nad_deque_drop(b);
}

// two rings holding the same elems start at different slots, so what is compared is the
// contents in ring order and never the buffers
static void test_eq_ignores_where_the_ring_starts() {
    constexpr int32_t want[4] = {10, 20, 30, 40};

    nad_Deque *straight = nullptr;
    NAD_TEST_OK(NAD_DEQUE_FROM_DATA(int32_t, want, 4, nad_al_default(), &straight));
    nad_Deque *wrapped = make_wrapped();

    TEST_ASSERT_TRUE(wraps(wrapped));
    TEST_ASSERT_FALSE(wraps(straight));
    TEST_ASSERT_TRUE(nad_deque_eq(straight, wrapped));
    TEST_ASSERT_TRUE(nad_deque_eq(wrapped, straight));
    TEST_ASSERT_TRUE(nad_deque_eq_by(wrapped, straight, nad_eq_i32));

    nad_deque_drop(straight);
    nad_deque_drop(wrapped);
}

// the same elems rotated by one: equal as multisets, unequal as deques
static void test_eq_is_order_sensitive() {
    nad_Deque *a = make_deque(4);
    nad_Deque *b = make_deque(4);

    const int32_t front = *NAD_DEQUE_FIRST_AS(int32_t, b);
    nad_deque_pop_front(b);
    push_back_int(b, front);

    TEST_ASSERT_EQUAL_size_t(nad_deque_len(a), nad_deque_len(b));
    TEST_ASSERT_FALSE(nad_deque_eq(a, b));

    nad_deque_drop(a);
    nad_deque_drop(b);
}

static void test_eq_by_asks_the_equality() {
    constexpr Pair lhs[2] = {{1, 10}, {2, 20}};
    constexpr Pair rhs[2] = {{1, 70}, {2, 80}};

    nad_Deque *a = nullptr;
    nad_Deque *b = nullptr;
    NAD_TEST_OK(NAD_DEQUE_FROM_DATA(Pair, lhs, 2, nad_al_default(), &a));
    NAD_TEST_OK(NAD_DEQUE_FROM_DATA(Pair, rhs, 2, nad_al_default(), &b));

    TEST_ASSERT_FALSE(nad_deque_eq(a, b));
    TEST_ASSERT_TRUE(nad_deque_eq_by(a, b, nad_test_pair_eq_a));

    nad_deque_drop(a);
    nad_deque_drop(b);
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

    return UNITY_END();
}
