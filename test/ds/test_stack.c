#include "trs/ds/stack.h"
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

static void push_int(trs_Stack *s, int32_t val) {
    TRS_TEST_OK(trs_stack_push(s, &val));
}

// int32_t stack over the default allocator, filled one push at a time
[[nodiscard]]
static trs_Stack *make_stack(const int32_t *src, size_t n) {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW(int32_t, trs_al_default(), &s));

    for (size_t i = 0; i < n; ++i) {
        push_int(s, src[i]);
    }
    return s;
}

// the same elems handed over in one go instead
[[nodiscard]]
static trs_Stack *make_stack_from(const int32_t *src, size_t n) {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_FROM_DATA(int32_t, src, n, trs_al_default(), &s));

    return s;
}

// 'want' is written bottom to top, the order the elems were pushed in, which is the
// order the span shows them in
static void assert_elems(const trs_Stack *s, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_stack_len(s));

    const trs_Span sp = trs_stack_to_span(s);
    TEST_ASSERT_EQUAL_size_t(n, sp.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), sp.elem_size);

    if (n > 0) {
        TEST_ASSERT_EQUAL_INT32_ARRAY(want, sp.data, n);
    }
}

// empties the stack through top + pop, checking at every step that the newest elem is
// the one on offer and that exactly one left. The order is checked here rather than in a
// test of its own because it must hold after EVERY pop, not just the last one. 'want' is
// bottom to top, so the drain walks it backwards
static void assert_drains(trs_Stack *s, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_stack_len(s));

    for (size_t i = n; i > 0; --i) {
        TEST_ASSERT_EQUAL_INT32(want[i - 1], *TRS_STACK_TOP_AS(int32_t, s));
        trs_stack_pop(s);

        TEST_ASSERT_EQUAL_size_t(i - 1, trs_stack_len(s));
    }
}

/* ========== lifetime ========== */

static void test_new_starts_empty() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW(int32_t, trs_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(0, trs_stack_len(s));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_stack_elem_size(s));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_stack_al(s));

    trs_stack_drop(s);
}

static void test_new_cap_reserves_without_length() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW_CAP(int32_t, 16, trs_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(0, trs_stack_len(s));
    TEST_ASSERT_EQUAL_size_t(16, trs_stack_cap(s));

    trs_stack_drop(s);
}

// the elems are taken in the order they are written, so the one written last is the one
// that comes off first
static void test_from_data_puts_the_last_elem_on_top() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    assert_elems(s, SPREAD, SPREAD_LEN);
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *TRS_STACK_TOP_AS(int32_t, s));

    trs_stack_drop(s);
}

static void test_from_data_empty_stays_empty() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_FROM_DATA(int32_t, nullptr, 0, trs_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(0, trs_stack_len(s));

    trs_stack_drop(s);
}

static void test_from_span_copies_the_view() {
    constexpr int32_t src[4] = {9, 8, 7, 6};

    trs_Stack *s = nullptr;
    TRS_TEST_OK(trs_stack_from_span(TRS_SPAN_FROM_DATA(int32_t, src, 4), trs_al_default(), &s));

    assert_elems(s, src, 4);
    TEST_ASSERT_EQUAL_INT32(6, *TRS_STACK_TOP_AS(int32_t, s));

    trs_stack_drop(s);
}

// an elem wider than a word must travel whole, not by its first field
static void test_from_data_copies_whole_elems() {
    constexpr Pair src[3] = {{1, 10}, {2, 20}, {3, 30}};

    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_FROM_DATA(Pair, src, 3, trs_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), trs_stack_elem_size(s));
    TEST_ASSERT_EQUAL_INT64(3, TRS_STACK_TOP_AS(Pair, s)->a);
    TEST_ASSERT_EQUAL_INT64(30, TRS_STACK_TOP_AS(Pair, s)->b);

    trs_stack_drop(s);
}

static void test_drop_null_is_noop() {
    trs_stack_drop(nullptr);
}

// three blocks go into a filled stack — the vec, its buffer, and the header that hides
// the vec — and drop must hand back all three. The default allocator would say nothing
// about it, so the count comes from a probe
static void test_drop_hands_back_everything_it_took() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_FROM_DATA(int32_t, SPREAD, SPREAD_LEN, &al, &s));
    TEST_ASSERT_EQUAL_size_t(3, probe.live);

    trs_stack_drop(s);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
    TEST_ASSERT_EQUAL_size_t(3, probe.dealloc_calls);
}

/* ========== lifo ========== */

static void test_pushes_leave_newest_first() {
    trs_Stack *s = make_stack(SPREAD, SPREAD_LEN);

    assert_drains(s, SPREAD, SPREAD_LEN);

    trs_stack_drop(s);
}

// growth relocates every elem, so the order has to survive a move of the whole buffer —
// this stack outgrows its first block several times over
static void test_order_survives_growth() {
    int32_t want[64];
    for (int32_t i = 0; i < 64; ++i) {
        want[i] = i * 3;
    }

    trs_Stack *s = make_stack(want, 64);

    TEST_ASSERT_TRUE(trs_stack_cap(s) >= 64);
    assert_drains(s, want, 64);

    trs_stack_drop(s);
}

// pushing and popping in step keeps the stack short while far more elems than it holds
// pass through
static void test_pushing_and_popping_in_step_stays_in_order() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW(int32_t, trs_al_default(), &s));

    push_int(s, 0);

    for (int32_t i = 1; i < 100; ++i) {
        push_int(s, i);
        TEST_ASSERT_EQUAL_INT32(i, *TRS_STACK_TOP_AS(int32_t, s));
        trs_stack_pop(s);

        TEST_ASSERT_EQUAL_INT32(0, *TRS_STACK_TOP_AS(int32_t, s));
        TEST_ASSERT_EQUAL_size_t(1, trs_stack_len(s));
    }

    trs_stack_drop(s);
}

static void test_pushes_after_a_full_drain_are_ordered_again() {
    trs_Stack *s = make_stack(SPREAD, SPREAD_LEN);
    assert_drains(s, SPREAD, SPREAD_LEN);

    constexpr int32_t again[4] = {41, 42, 43, 44};
    for (size_t i = 0; i < 4; ++i) {
        push_int(s, again[i]);
    }
    assert_drains(s, again, 4);

    trs_stack_drop(s);
}

// equal elems are not one elem: every copy pushed must come back
static void test_duplicates_all_come_back() {
    constexpr int32_t src[6] = {7, 7, 7, 7, 7, 7};

    trs_Stack *s = make_stack(src, 6);

    assert_drains(s, src, 6);

    trs_stack_drop(s);
}

// what a pop uncovers is the elem pushed just before the one it removed
static void test_a_pop_uncovers_the_elem_below() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, trs_al_default(), &s, 1, 2, 3));

    TEST_ASSERT_EQUAL_INT32(3, *TRS_STACK_TOP_AS(int32_t, s));
    trs_stack_pop(s);
    TEST_ASSERT_EQUAL_INT32(2, *TRS_STACK_TOP_AS(int32_t, s));
    trs_stack_pop(s);
    TEST_ASSERT_EQUAL_INT32(1, *TRS_STACK_TOP_AS(int32_t, s));

    trs_stack_drop(s);
}

/* ========== access ========== */

static void test_top_reads_without_removing() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *TRS_STACK_TOP_AS(int32_t, s));
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *TRS_STACK_TOP_AS(int32_t, s));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_stack_len(s));

    trs_stack_drop(s);
}

static void test_top_follows_the_last_push() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW(int32_t, trs_al_default(), &s));

    for (int32_t i = 0; i < 8; ++i) {
        push_int(s, i);
        TEST_ASSERT_EQUAL_INT32(i, *TRS_STACK_TOP_AS(int32_t, s));
    }

    trs_stack_drop(s);
}

// there is no order over the elems to break, so writing through the top is legal —
// the stack only rules where they enter and leave
static void test_the_top_is_writable() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, trs_al_default(), &s, 1, 2, 3));

    *TRS_STACK_TOP_MUT_AS(int32_t, s) = -3;

    constexpr int32_t want[3] = {1, 2, -3};
    assert_elems(s, want, 3);

    trs_stack_drop(s);
}

// on a stack of one the top is also the bottom, which is where the span starts
static void test_the_top_of_a_single_elem_stack_is_where_the_span_starts() {
    trs_Stack *s = make_stack_from(SPREAD, 1);

    TEST_ASSERT_EQUAL_PTR(trs_stack_to_span(s).data, trs_stack_top(s));
    TEST_ASSERT_EQUAL_PTR(trs_stack_top(s), trs_stack_top_mut(s));

    trs_stack_drop(s);
}

/* ========== info ========== */

static void test_len_follows_push_and_pop() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW(int32_t, trs_al_default(), &s));

    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_size_t(i, trs_stack_len(s));
        push_int(s, (int32_t) i);
    }
    for (size_t i = 5; i > 0; --i) {
        TEST_ASSERT_EQUAL_size_t(i, trs_stack_len(s));
        trs_stack_pop(s);
    }
    TEST_ASSERT_EQUAL_size_t(0, trs_stack_len(s));

    trs_stack_drop(s);
}

// popping hands nothing back to the allocator: the room stays for the next push
static void test_pop_leaves_the_capacity_alone() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const size_t cap = trs_stack_cap(s);

    trs_stack_pop(s);
    trs_stack_pop(s);

    TEST_ASSERT_EQUAL_size_t(cap, trs_stack_cap(s));

    trs_stack_drop(s);
}

/* ========== mods ========== */

static void test_clear_empties_without_giving_back_the_room() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const size_t cap = trs_stack_cap(s);

    trs_stack_clear(s);

    TEST_ASSERT_EQUAL_size_t(0, trs_stack_len(s));
    TEST_ASSERT_EQUAL_size_t(cap, trs_stack_cap(s));

    trs_stack_drop(s);
}

static void test_clear_leaves_a_usable_stack() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    trs_stack_clear(s);

    constexpr int32_t again[3] = {1, 2, 3};
    for (size_t i = 0; i < 3; ++i) {
        push_int(s, again[i]);
    }
    assert_drains(s, again, 3);

    trs_stack_drop(s);
}

static void test_reserve_grows_the_room_only() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    TRS_TEST_OK(trs_stack_reserve(s, 100));

    TEST_ASSERT_TRUE(trs_stack_cap(s) >= 100);
    assert_elems(s, SPREAD, SPREAD_LEN);

    trs_stack_drop(s);
}

static void test_reserve_below_the_capacity_changes_nothing() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const size_t cap = trs_stack_cap(s);

    TRS_TEST_OK(trs_stack_reserve(s, 1));

    TEST_ASSERT_EQUAL_size_t(cap, trs_stack_cap(s));

    trs_stack_drop(s);
}

static void test_shrink_to_fit_keeps_the_order() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    TRS_TEST_OK(trs_stack_reserve(s, 100));

    TRS_TEST_OK(trs_stack_shrink_to_fit(s));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_stack_cap(s));
    assert_elems(s, SPREAD, SPREAD_LEN);
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *TRS_STACK_TOP_AS(int32_t, s));

    trs_stack_drop(s);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    trs_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(trs_stack_copy(src, &dst));

    trs_stack_pop(dst);
    push_int(dst, 777);

    assert_elems(src, SPREAD, SPREAD_LEN);
    TEST_ASSERT_EQUAL_INT32(777, *TRS_STACK_TOP_AS(int32_t, dst));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_stack_len(dst));

    trs_stack_drop(src);
    trs_stack_drop(dst);
}

static void test_copy_inherits_the_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Stack *src = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, arena, &src, 5, 1, 3));

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(trs_stack_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_stack_al(dst));
    TEST_ASSERT_EQUAL_INT32(3, *TRS_STACK_TOP_AS(int32_t, dst));

    trs_stack_drop(src);
    trs_stack_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_copy_with_builds_on_the_given_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(trs_stack_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_stack_al(dst));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_stack_al(src));
    TEST_ASSERT_TRUE(trs_stack_eq(src, dst));

    // the header goes to the same allocator as the elems, so the copy outlives the source
    trs_stack_drop(src);
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *TRS_STACK_TOP_AS(int32_t, dst));

    trs_stack_drop(dst);
    trs_al_arena_drop(arena);
}

// the blocks are asked of the allocator the copy is going to, not of the source's
static void test_copy_with_reports_an_exhausted_target_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    trs_Stack *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_stack_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_stack_len(src));

    trs_stack_drop(src);
    trs_al_arena_drop(arena);
}

static void test_move_assign_hands_over_the_contents_on_one_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Stack *src = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, &al, &src, 1, 2, 3));

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, &al, &dst, 9));

    const size_t requests = trs_test_probe_requests(&probe);
    TRS_TEST_OK(trs_stack_move_assign(src, dst));

    // nothing was asked of the allocator: the vec's block changed hands
    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    TEST_ASSERT_EQUAL_size_t(3, trs_stack_len(dst));
    TEST_ASSERT_EQUAL_INT32(3, *TRS_STACK_TOP_AS(int32_t, dst));
    TEST_ASSERT_EQUAL_size_t(0, trs_stack_len(src));

    trs_stack_drop(src);
    trs_stack_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_move_assign_across_allocators_empties_the_source() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, arena, &dst, 9));

    TRS_TEST_OK(trs_stack_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_stack_len(dst));
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *TRS_STACK_TOP_AS(int32_t, dst));
    TEST_ASSERT_EQUAL_PTR(arena, trs_stack_al(dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_stack_len(src));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_stack_al(src));

    trs_stack_drop(src);
    trs_stack_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_across_allocators_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, arena, &dst, 9));
    trs_test_arena_leave(arena, 0);

    trs_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_stack_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_stack_len(src));
    TEST_ASSERT_EQUAL_size_t(1, trs_stack_len(dst));
    TEST_ASSERT_EQUAL_INT32(9, *TRS_STACK_TOP_AS(int32_t, dst));

    trs_stack_drop(src);
    trs_stack_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_of_itself_changes_nothing() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    TRS_TEST_OK(trs_stack_move_assign(s, s));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, trs_stack_len(s));
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *TRS_STACK_TOP_AS(int32_t, s));

    trs_stack_drop(s);
}

static void test_copy_drains_the_same_as_its_source() {
    trs_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(trs_stack_copy(src, &dst));

    assert_drains(dst, SPREAD, SPREAD_LEN);

    trs_stack_drop(src);
    trs_stack_drop(dst);
}

static void test_copy_of_empty_stays_empty() {
    trs_Stack *src = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW(int32_t, trs_al_default(), &src));

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(trs_stack_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_stack_len(dst));

    trs_stack_drop(src);
    trs_stack_drop(dst);
}

static void test_copy_assign_overwrites_the_target() {
    trs_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    trs_Stack *dst = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, trs_al_default(), &dst, 1, 2, 3));

    TRS_TEST_OK(trs_stack_copy_assign(src, dst));

    assert_elems(dst, SPREAD, SPREAD_LEN);
    assert_elems(src, SPREAD, SPREAD_LEN);

    trs_stack_drop(src);
    trs_stack_drop(dst);
}

static void test_copy_assign_self_is_noop() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    TRS_TEST_OK(trs_stack_copy_assign(s, s));

    assert_elems(s, SPREAD, SPREAD_LEN);

    trs_stack_drop(s);
}

/* ========== to span ========== */

// the span runs bottom to top, which is push order — the reverse of the order the elems
// will come off in
static void test_to_span_shows_bottom_to_top() {
    trs_Stack *s = make_stack(SPREAD, SPREAD_LEN);
    const trs_Span sp = trs_stack_to_span(s);

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, sp.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), sp.elem_size);
    TEST_ASSERT_EQUAL_INT32_ARRAY(SPREAD, sp.data, SPREAD_LEN);

    trs_stack_drop(s);
}

// the top is the LAST elem of the span, not the first — that is the whole difference
// between this and a queue
static void test_to_span_ends_at_the_top() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const trs_Span sp = trs_stack_to_span(s);

    TEST_ASSERT_EQUAL_PTR(trs_span_get(sp, sp.len - 1), trs_stack_top(s));

    trs_stack_drop(s);
}

static void test_to_span_follows_a_pop() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    trs_stack_pop(s);
    trs_stack_pop(s);

    assert_elems(s, SPREAD, SPREAD_LEN - 2);

    trs_stack_drop(s);
}

static void test_to_span_of_empty_has_no_elems() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW(int32_t, trs_al_default(), &s));

    const trs_Span sp = trs_stack_to_span(s);

    TEST_ASSERT_EQUAL_size_t(0, sp.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), sp.elem_size);

    trs_stack_drop(s);
}

// the point of the one way bridge: algo reads the span, and the arrival order that gives
// the stack its meaning is not something algo can rearrange — there is no mutable view
static void test_the_span_reaches_algo_and_the_stack_is_untouched() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const trs_Span sp = trs_stack_to_span(s);

    TEST_ASSERT_EQUAL_size_t(2, trs_span_max_elem(sp, trs_cmp_i32));
    TEST_ASSERT_FALSE(trs_span_is_sorted(sp, trs_cmp_i32));

    assert_elems(s, SPREAD, SPREAD_LEN);

    trs_stack_drop(s);
}

/* ========== swap ========== */

static void test_swap_exchanges_the_elems() {
    trs_Stack *a = nullptr;
    trs_Stack *b = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, trs_al_default(), &a, 1, 2, 3));
    TRS_TEST_OK(TRS_STACK_OF(int32_t, trs_al_default(), &b, 10, 20));

    trs_stack_swap(a, b);

    constexpr int32_t want_a[2] = {10, 20};
    constexpr int32_t want_b[3] = {1, 2, 3};
    assert_elems(a, want_a, 2);
    assert_elems(b, want_b, 3);
    TEST_ASSERT_EQUAL_INT32(20, *TRS_STACK_TOP_AS(int32_t, a));
    TEST_ASSERT_EQUAL_INT32(3, *TRS_STACK_TOP_AS(int32_t, b));

    trs_stack_drop(a);
    trs_stack_drop(b);
}

static void test_swap_self_is_noop() {
    trs_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    trs_stack_swap(s, s);

    assert_elems(s, SPREAD, SPREAD_LEN);

    trs_stack_drop(s);
}

/* ========== failures ========== */

static void test_new_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_Stack *s = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_STACK_NEW(int32_t, arena, &s));
    TEST_ASSERT_NULL(s);

    trs_al_arena_drop(arena);
}

static void test_from_data_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 128);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Stack *s = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_STACK_FROM_DATA(int32_t, SPREAD, 1000, arena, &s));
    TEST_ASSERT_NULL(s);

    trs_al_arena_drop(arena);
}

// a refused push must leave the stack exactly as it was — not one elem taller with a
// hole on top
static void test_push_reports_an_exhausted_arena_and_changes_nothing() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, arena, &s, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    constexpr int32_t val = 100;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_stack_push(s, &val));

    constexpr int32_t want[3] = {5, 1, 3};
    assert_elems(s, want, 3);
    TEST_ASSERT_EQUAL_INT32(3, *TRS_STACK_TOP_AS(int32_t, s));

    trs_stack_drop(s);
    trs_al_arena_drop(arena);
}

static void test_copy_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Stack *src = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, arena, &src, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    trs_Stack *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_stack_copy(src, &dst));
    TEST_ASSERT_NULL(dst);

    trs_stack_drop(src);
    trs_al_arena_drop(arena);
}

static void test_reserve_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, arena, &s, 5, 1, 3));
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_stack_reserve(s, 1000));

    constexpr int32_t want[3] = {5, 1, 3};
    assert_elems(s, want, 3);

    trs_stack_drop(s);
    trs_al_arena_drop(arena);
}

// The stack is built in two steps, the vec and then the header that hides it. When the
// second one is refused the first must not be stranded: the probe counts what is still
// live, and an arena would hide the leak because it frees everything at once
static void test_a_refused_header_frees_the_vec() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 1);

    trs_Stack *s = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_STACK_NEW(int32_t, &al, &s));

    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the same, one allocation later: from_data takes a buffer as well
static void test_a_refused_header_frees_a_filled_vec() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 2);

    trs_Stack *s = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_STACK_FROM_DATA(int32_t, SPREAD, SPREAD_LEN, &al, &s));

    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== compare ========== */

static void test_eq_matches_the_same_elems() {
    constexpr int32_t src[3] = {7, 8, 9};

    trs_Stack *a = make_stack(src, 3);
    trs_Stack *b = make_stack_from(src, 3);

    TEST_ASSERT_TRUE(trs_stack_eq(a, a));
    TEST_ASSERT_TRUE(trs_stack_eq(a, b));
    TEST_ASSERT_TRUE(trs_stack_eq(b, a));
    TEST_ASSERT_TRUE(trs_stack_eq_by(a, b, trs_eq_i32));

    trs_stack_drop(a);
    trs_stack_drop(b);
}

static void test_eq_parts_one_differing_elem() {
    constexpr int32_t lhs[3] = {7, 8, 9};
    constexpr int32_t rhs[3] = {7, 8, 99};

    trs_Stack *a = make_stack(lhs, 3);
    trs_Stack *b = make_stack(rhs, 3);

    TEST_ASSERT_FALSE(trs_stack_eq(a, b));
    TEST_ASSERT_FALSE(trs_stack_eq_by(a, b, trs_eq_i32));

    trs_stack_drop(a);
    trs_stack_drop(b);
}

static void test_eq_parts_different_lengths() {
    constexpr int32_t src[3] = {7, 8, 9};

    trs_Stack *a = make_stack(src, 3);
    trs_Stack *shorter = make_stack(src, 2);

    TEST_ASSERT_FALSE(trs_stack_eq(a, shorter));
    TEST_ASSERT_FALSE(trs_stack_eq(shorter, a));

    trs_stack_drop(a);
    trs_stack_drop(shorter);
}

static void test_eq_of_two_empties() {
    constexpr int32_t src[1] = {7};

    trs_Stack *a = make_stack(src, 0);
    trs_Stack *b = make_stack_from(src, 0);
    trs_Stack *one = make_stack(src, 1);

    TEST_ASSERT_TRUE(trs_stack_eq(a, b));
    TEST_ASSERT_TRUE(trs_stack_eq_by(a, b, trs_eq_i32));
    TEST_ASSERT_FALSE(trs_stack_eq(a, one));

    trs_stack_drop(a);
    trs_stack_drop(b);
    trs_stack_drop(one);
}

// a popped elem is gone even though its bytes are still in the vec underneath
static void test_eq_forgets_a_popped_elem() {
    constexpr int32_t lhs[2] = {7, 8};
    constexpr int32_t rhs[3] = {7, 8, 9};

    trs_Stack *a = make_stack(lhs, 2);
    trs_Stack *b = make_stack(rhs, 3);

    TEST_ASSERT_FALSE(trs_stack_eq(a, b));
    trs_stack_pop(b);
    TEST_ASSERT_TRUE(trs_stack_eq(a, b));

    trs_stack_drop(a);
    trs_stack_drop(b);
}

static void test_eq_by_asks_the_equality() {
    constexpr Pair lhs[2] = {{1, 10}, {2, 20}};
    constexpr Pair rhs[2] = {{1, 70}, {2, 80}};

    trs_Stack *a = nullptr;
    trs_Stack *b = nullptr;
    TRS_TEST_OK(TRS_STACK_FROM_DATA(Pair, lhs, 2, trs_al_default(), &a));
    TRS_TEST_OK(TRS_STACK_FROM_DATA(Pair, rhs, 2, trs_al_default(), &b));

    TEST_ASSERT_FALSE(trs_stack_eq(a, b));
    TEST_ASSERT_TRUE(trs_stack_eq_by(a, b, trs_test_pair_eq_a));

    trs_stack_drop(a);
    trs_stack_drop(b);
}

/* ========== into ========== */

// the vec was there all along: taking it copies nothing and leaves the elems where they
// were, bottom to top
static void test_into_vec_hands_the_elems_over() {
    constexpr int32_t src[3] = {7, 8, 9};
    trs_Stack *s = make_stack(src, 3);

    const void *before = trs_stack_to_span(s).data;
    const size_t cap = trs_stack_cap(s);

    trs_Vec *v = trs_stack_into_vec(s);

    TEST_ASSERT_EQUAL_PTR(before, trs_vec_data(v));
    TEST_ASSERT_EQUAL_size_t(3, trs_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(cap, trs_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_vec_al(v));

    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_INT32(src[i], *TRS_VEC_GET_AS(int32_t, v, i));
    }

    trs_vec_drop(v);
}

static void test_into_vec_of_an_empty_stack() {
    constexpr int32_t src[1] = {7};
    trs_Stack *s = make_stack(src, 0);

    trs_Vec *v = trs_stack_into_vec(s);

    TEST_ASSERT_EQUAL_size_t(0, trs_vec_len(v));

    trs_vec_drop(v);
}

// only the adapter's own header goes back; dropping the vec afterwards squares the books
static void test_into_vec_releases_the_header_alone() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_NEW(int32_t, &al, &s));

    // the last block the constructor took is the adapter's own header, so this is what
    // into has to hand back, and with the size it was taken as
    const size_t header = probe.last_alloc_size;

    TRS_TEST_OK(TRS_STACK_PUSH(int32_t, s, 1));
    const size_t live = probe.live;

    trs_Vec *v = trs_stack_into_vec(s);

    TEST_ASSERT_EQUAL_size_t(live - 1, probe.live);
    TEST_ASSERT_EQUAL_size_t(header, probe.last_dealloc_size);

    trs_vec_drop(v);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== print ========== */

// a printer writes to a stream, so a case has to read one back. tmpfile is the portable
// way, the same one test/core/test_print.c takes
static void assert_prints(const char *expected, const trs_Stack *s) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);

    trs_stack_fprint(s, stream, trs_fprint_i32);
    rewind(stream);

    char buf[128];
    const size_t n = fread(buf, 1, sizeof buf - 1, stream);
    buf[n] = '\0';
    fclose(stream);

    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_fprint_writes_the_elems() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, trs_al_default(), &s, 5, 3, 1));

    assert_prints("[5, 3, 1]\n", s);

    trs_stack_drop(s);
}

static void test_fprint_of_a_single_elem_has_no_separator() {
    trs_Stack *s = nullptr;
    TRS_TEST_OK(TRS_STACK_OF(int32_t, trs_al_default(), &s, 7));

    assert_prints("[7]\n", s);

    trs_stack_drop(s);
}

static void test_fprint_of_an_empty_stack() {
    trs_Stack *s = make_stack(nullptr, 0);

    assert_prints("[]\n", s);

    trs_stack_drop(s);
}

// the stdout twin takes no stream, and C has no portable way to capture one and give it
// back — so a case can only say that it runs and reaches the same printer
static void test_print_writes_to_stdout() {
    constexpr int32_t src[3] = {1, 2, 3};
    trs_Stack *s = make_stack(src, 3);

    trs_stack_print(s, trs_fprint_i32);

    trs_stack_drop(s);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_empty);
    RUN_TEST(test_new_cap_reserves_without_length);
    RUN_TEST(test_from_data_puts_the_last_elem_on_top);
    RUN_TEST(test_from_data_empty_stays_empty);
    RUN_TEST(test_from_span_copies_the_view);
    RUN_TEST(test_from_data_copies_whole_elems);
    RUN_TEST(test_drop_null_is_noop);
    RUN_TEST(test_drop_hands_back_everything_it_took);

    RUN_TEST(test_pushes_leave_newest_first);
    RUN_TEST(test_order_survives_growth);
    RUN_TEST(test_pushing_and_popping_in_step_stays_in_order);
    RUN_TEST(test_pushes_after_a_full_drain_are_ordered_again);
    RUN_TEST(test_duplicates_all_come_back);
    RUN_TEST(test_a_pop_uncovers_the_elem_below);

    RUN_TEST(test_top_reads_without_removing);
    RUN_TEST(test_top_follows_the_last_push);
    RUN_TEST(test_the_top_is_writable);
    RUN_TEST(test_the_top_of_a_single_elem_stack_is_where_the_span_starts);

    RUN_TEST(test_len_follows_push_and_pop);
    RUN_TEST(test_pop_leaves_the_capacity_alone);

    RUN_TEST(test_clear_empties_without_giving_back_the_room);
    RUN_TEST(test_clear_leaves_a_usable_stack);
    RUN_TEST(test_reserve_grows_the_room_only);
    RUN_TEST(test_reserve_below_the_capacity_changes_nothing);
    RUN_TEST(test_shrink_to_fit_keeps_the_order);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_inherits_the_allocator);
    RUN_TEST(test_copy_with_builds_on_the_given_allocator);
    RUN_TEST(test_copy_with_reports_an_exhausted_target_arena);
    RUN_TEST(test_move_assign_hands_over_the_contents_on_one_allocator);
    RUN_TEST(test_move_assign_across_allocators_empties_the_source);
    RUN_TEST(test_move_assign_across_allocators_reports_an_exhausted_arena);
    RUN_TEST(test_move_assign_of_itself_changes_nothing);
    RUN_TEST(test_copy_drains_the_same_as_its_source);
    RUN_TEST(test_copy_of_empty_stays_empty);
    RUN_TEST(test_copy_assign_overwrites_the_target);
    RUN_TEST(test_copy_assign_self_is_noop);

    RUN_TEST(test_to_span_shows_bottom_to_top);
    RUN_TEST(test_to_span_ends_at_the_top);
    RUN_TEST(test_to_span_follows_a_pop);
    RUN_TEST(test_to_span_of_empty_has_no_elems);
    RUN_TEST(test_the_span_reaches_algo_and_the_stack_is_untouched);

    RUN_TEST(test_swap_exchanges_the_elems);
    RUN_TEST(test_swap_self_is_noop);

    RUN_TEST(test_new_reports_an_exhausted_arena);
    RUN_TEST(test_from_data_reports_an_exhausted_arena);
    RUN_TEST(test_push_reports_an_exhausted_arena_and_changes_nothing);
    RUN_TEST(test_copy_reports_an_exhausted_arena);
    RUN_TEST(test_reserve_reports_an_exhausted_arena);
    RUN_TEST(test_a_refused_header_frees_the_vec);
    RUN_TEST(test_a_refused_header_frees_a_filled_vec);


    RUN_TEST(test_eq_matches_the_same_elems);
    RUN_TEST(test_eq_parts_one_differing_elem);
    RUN_TEST(test_eq_parts_different_lengths);
    RUN_TEST(test_eq_of_two_empties);
    RUN_TEST(test_eq_forgets_a_popped_elem);
    RUN_TEST(test_eq_by_asks_the_equality);


    RUN_TEST(test_into_vec_hands_the_elems_over);
    RUN_TEST(test_into_vec_of_an_empty_stack);
    RUN_TEST(test_into_vec_releases_the_header_alone);

    RUN_TEST(test_fprint_writes_the_elems);
    RUN_TEST(test_fprint_of_a_single_elem_has_no_separator);
    RUN_TEST(test_fprint_of_an_empty_stack);
    RUN_TEST(test_print_writes_to_stdout);

    return UNITY_END();
}
