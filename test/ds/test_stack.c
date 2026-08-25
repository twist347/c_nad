#include "nad/ds/stack.h"
#include "nad/algo/search.h"
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

static constexpr int32_t SPREAD[] = {5, 1, 9, 9, 3, 7, 2, 8, 3, 6, 0, 4, 9, 1};
static constexpr size_t SPREAD_LEN = sizeof(SPREAD) / sizeof(SPREAD[0]);

static void push_int(nad_Stack *s, int32_t val) {
    NAD_TEST_OK(nad_stack_push(s, &val));
}

// int32_t stack over the default allocator, filled one push at a time
[[nodiscard]]
static nad_Stack *make_stack(const int32_t *src, size_t n) {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW(int32_t, nad_al_default(), &s));

    for (size_t i = 0; i < n; ++i) {
        push_int(s, src[i]);
    }
    return s;
}

// the same elems handed over in one go instead
[[nodiscard]]
static nad_Stack *make_stack_from(const int32_t *src, size_t n) {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_FROM_DATA(int32_t, src, n, nad_al_default(), &s));

    return s;
}

// 'want' is written bottom to top, the order the elems were pushed in, which is the
// order the span shows them in
static void assert_elems(const nad_Stack *s, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, nad_stack_len(s));

    const nad_Span sp = nad_stack_to_span(s);
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
static void assert_drains(nad_Stack *s, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, nad_stack_len(s));

    for (size_t i = n; i > 0; --i) {
        TEST_ASSERT_EQUAL_INT32(want[i - 1], *NAD_STACK_TOP_AS(int32_t, s));
        nad_stack_pop(s);

        TEST_ASSERT_EQUAL_size_t(i - 1, nad_stack_len(s));
    }
}

/* ========== lifetime ========== */

static void test_new_starts_empty() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW(int32_t, nad_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(0, nad_stack_len(s));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_stack_elem_size(s));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_stack_al(s));

    nad_stack_drop(s);
}

static void test_new_cap_reserves_without_length() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW_CAP(int32_t, 16, nad_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(0, nad_stack_len(s));
    TEST_ASSERT_EQUAL_size_t(16, nad_stack_cap(s));

    nad_stack_drop(s);
}

// the elems are taken in the order they are written, so the one written last is the one
// that comes off first
static void test_from_data_puts_the_last_elem_on_top() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    assert_elems(s, SPREAD, SPREAD_LEN);
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *NAD_STACK_TOP_AS(int32_t, s));

    nad_stack_drop(s);
}

static void test_from_data_empty_stays_empty() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_FROM_DATA(int32_t, nullptr, 0, nad_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(0, nad_stack_len(s));

    nad_stack_drop(s);
}

static void test_from_span_copies_the_view() {
    constexpr int32_t src[4] = {9, 8, 7, 6};

    nad_Stack *s = nullptr;
    NAD_TEST_OK(nad_stack_from_span(NAD_SPAN_NEW(int32_t, src, 4), nad_al_default(), &s));

    assert_elems(s, src, 4);
    TEST_ASSERT_EQUAL_INT32(6, *NAD_STACK_TOP_AS(int32_t, s));

    nad_stack_drop(s);
}

// an elem wider than a word must travel whole, not by its first field
static void test_from_data_copies_whole_elems() {
    constexpr Pair src[3] = {{1, 10}, {2, 20}, {3, 30}};

    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_FROM_DATA(Pair, src, 3, nad_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), nad_stack_elem_size(s));
    TEST_ASSERT_EQUAL_INT64(3, NAD_STACK_TOP_AS(Pair, s)->a);
    TEST_ASSERT_EQUAL_INT64(30, NAD_STACK_TOP_AS(Pair, s)->b);

    nad_stack_drop(s);
}

static void test_drop_null_is_noop() {
    nad_stack_drop(nullptr);
}

// three blocks go into a filled stack — the vec, its buffer, and the header that hides
// the vec — and drop must hand back all three. The default allocator would say nothing
// about it, so the count comes from a probe
static void test_drop_hands_back_everything_it_took() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_FROM_DATA(int32_t, SPREAD, SPREAD_LEN, &al, &s));
    TEST_ASSERT_EQUAL_size_t(3, probe.live);

    nad_stack_drop(s);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
    TEST_ASSERT_EQUAL_size_t(3, probe.dealloc_calls);
}

/* ========== lifo ========== */

static void test_pushes_leave_newest_first() {
    nad_Stack *s = make_stack(SPREAD, SPREAD_LEN);

    assert_drains(s, SPREAD, SPREAD_LEN);

    nad_stack_drop(s);
}

// growth relocates every elem, so the order has to survive a move of the whole buffer —
// this stack outgrows its first block several times over
static void test_order_survives_growth() {
    int32_t want[64];
    for (int32_t i = 0; i < 64; ++i) {
        want[i] = i * 3;
    }

    nad_Stack *s = make_stack(want, 64);

    TEST_ASSERT_TRUE(nad_stack_cap(s) >= 64);
    assert_drains(s, want, 64);

    nad_stack_drop(s);
}

// pushing and popping in step keeps the stack short while far more elems than it holds
// pass through
static void test_pushing_and_popping_in_step_stays_in_order() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW(int32_t, nad_al_default(), &s));

    push_int(s, 0);

    for (int32_t i = 1; i < 100; ++i) {
        push_int(s, i);
        TEST_ASSERT_EQUAL_INT32(i, *NAD_STACK_TOP_AS(int32_t, s));
        nad_stack_pop(s);

        TEST_ASSERT_EQUAL_INT32(0, *NAD_STACK_TOP_AS(int32_t, s));
        TEST_ASSERT_EQUAL_size_t(1, nad_stack_len(s));
    }

    nad_stack_drop(s);
}

static void test_pushes_after_a_full_drain_are_ordered_again() {
    nad_Stack *s = make_stack(SPREAD, SPREAD_LEN);
    assert_drains(s, SPREAD, SPREAD_LEN);

    constexpr int32_t again[4] = {41, 42, 43, 44};
    for (size_t i = 0; i < 4; ++i) {
        push_int(s, again[i]);
    }
    assert_drains(s, again, 4);

    nad_stack_drop(s);
}

// equal elems are not one elem: every copy pushed must come back
static void test_duplicates_all_come_back() {
    constexpr int32_t src[6] = {7, 7, 7, 7, 7, 7};

    nad_Stack *s = make_stack(src, 6);

    assert_drains(s, src, 6);

    nad_stack_drop(s);
}

// what a pop uncovers is the elem pushed just before the one it removed
static void test_a_pop_uncovers_the_elem_below() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, nad_al_default(), &s, 1, 2, 3));

    TEST_ASSERT_EQUAL_INT32(3, *NAD_STACK_TOP_AS(int32_t, s));
    nad_stack_pop(s);
    TEST_ASSERT_EQUAL_INT32(2, *NAD_STACK_TOP_AS(int32_t, s));
    nad_stack_pop(s);
    TEST_ASSERT_EQUAL_INT32(1, *NAD_STACK_TOP_AS(int32_t, s));

    nad_stack_drop(s);
}

/* ========== access ========== */

static void test_top_reads_without_removing() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *NAD_STACK_TOP_AS(int32_t, s));
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *NAD_STACK_TOP_AS(int32_t, s));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, nad_stack_len(s));

    nad_stack_drop(s);
}

static void test_top_follows_the_last_push() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW(int32_t, nad_al_default(), &s));

    for (int32_t i = 0; i < 8; ++i) {
        push_int(s, i);
        TEST_ASSERT_EQUAL_INT32(i, *NAD_STACK_TOP_AS(int32_t, s));
    }

    nad_stack_drop(s);
}

// there is no order over the elems to break, so writing through the top is legal —
// the stack only rules where they enter and leave
static void test_the_top_is_writable() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, nad_al_default(), &s, 1, 2, 3));

    *NAD_STACK_TOP_MUT_AS(int32_t, s) = -3;

    constexpr int32_t want[3] = {1, 2, -3};
    assert_elems(s, want, 3);

    nad_stack_drop(s);
}

// on a stack of one the top is also the bottom, which is where the span starts
static void test_the_top_of_a_single_elem_stack_is_where_the_span_starts() {
    nad_Stack *s = make_stack_from(SPREAD, 1);

    TEST_ASSERT_EQUAL_PTR(nad_stack_to_span(s).data, nad_stack_top(s));
    TEST_ASSERT_EQUAL_PTR(nad_stack_top(s), nad_stack_top_mut(s));

    nad_stack_drop(s);
}

/* ========== info ========== */

static void test_len_follows_push_and_pop() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW(int32_t, nad_al_default(), &s));

    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_size_t(i, nad_stack_len(s));
        push_int(s, (int32_t) i);
    }
    for (size_t i = 5; i > 0; --i) {
        TEST_ASSERT_EQUAL_size_t(i, nad_stack_len(s));
        nad_stack_pop(s);
    }
    TEST_ASSERT_EQUAL_size_t(0, nad_stack_len(s));

    nad_stack_drop(s);
}

// popping hands nothing back to the allocator: the room stays for the next push
static void test_pop_leaves_the_capacity_alone() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const size_t cap = nad_stack_cap(s);

    nad_stack_pop(s);
    nad_stack_pop(s);

    TEST_ASSERT_EQUAL_size_t(cap, nad_stack_cap(s));

    nad_stack_drop(s);
}

/* ========== mods ========== */

static void test_clear_empties_without_giving_back_the_room() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const size_t cap = nad_stack_cap(s);

    nad_stack_clear(s);

    TEST_ASSERT_EQUAL_size_t(0, nad_stack_len(s));
    TEST_ASSERT_EQUAL_size_t(cap, nad_stack_cap(s));

    nad_stack_drop(s);
}

static void test_clear_leaves_a_usable_stack() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    nad_stack_clear(s);

    constexpr int32_t again[3] = {1, 2, 3};
    for (size_t i = 0; i < 3; ++i) {
        push_int(s, again[i]);
    }
    assert_drains(s, again, 3);

    nad_stack_drop(s);
}

static void test_reserve_grows_the_room_only() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    NAD_TEST_OK(nad_stack_reserve(s, 100));

    TEST_ASSERT_TRUE(nad_stack_cap(s) >= 100);
    assert_elems(s, SPREAD, SPREAD_LEN);

    nad_stack_drop(s);
}

static void test_reserve_below_the_capacity_changes_nothing() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const size_t cap = nad_stack_cap(s);

    NAD_TEST_OK(nad_stack_reserve(s, 1));

    TEST_ASSERT_EQUAL_size_t(cap, nad_stack_cap(s));

    nad_stack_drop(s);
}

static void test_shrink_to_fit_keeps_the_order() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    NAD_TEST_OK(nad_stack_reserve(s, 100));

    NAD_TEST_OK(nad_stack_shrink_to_fit(s));

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, nad_stack_cap(s));
    assert_elems(s, SPREAD, SPREAD_LEN);
    TEST_ASSERT_EQUAL_INT32(SPREAD[SPREAD_LEN - 1], *NAD_STACK_TOP_AS(int32_t, s));

    nad_stack_drop(s);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    nad_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    nad_Stack *dst = nullptr;
    NAD_TEST_OK(nad_stack_copy(src, &dst));

    nad_stack_pop(dst);
    push_int(dst, 777);

    assert_elems(src, SPREAD, SPREAD_LEN);
    TEST_ASSERT_EQUAL_INT32(777, *NAD_STACK_TOP_AS(int32_t, dst));
    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, nad_stack_len(dst));

    nad_stack_drop(src);
    nad_stack_drop(dst);
}

static void test_copy_inherits_the_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Stack *src = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, arena, &src, 5, 1, 3));

    nad_Stack *dst = nullptr;
    NAD_TEST_OK(nad_stack_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, nad_stack_al(dst));
    TEST_ASSERT_EQUAL_INT32(3, *NAD_STACK_TOP_AS(int32_t, dst));

    nad_stack_drop(src);
    nad_stack_drop(dst);
    nad_al_arena_drop(arena);
}

static void test_copy_drains_the_same_as_its_source() {
    nad_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    nad_Stack *dst = nullptr;
    NAD_TEST_OK(nad_stack_copy(src, &dst));

    assert_drains(dst, SPREAD, SPREAD_LEN);

    nad_stack_drop(src);
    nad_stack_drop(dst);
}

static void test_copy_of_empty_stays_empty() {
    nad_Stack *src = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW(int32_t, nad_al_default(), &src));

    nad_Stack *dst = nullptr;
    NAD_TEST_OK(nad_stack_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(0, nad_stack_len(dst));

    nad_stack_drop(src);
    nad_stack_drop(dst);
}

static void test_copy_assign_overwrites_the_target() {
    nad_Stack *src = make_stack_from(SPREAD, SPREAD_LEN);

    nad_Stack *dst = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, nad_al_default(), &dst, 1, 2, 3));

    NAD_TEST_OK(nad_stack_copy_assign(src, dst));

    assert_elems(dst, SPREAD, SPREAD_LEN);
    assert_elems(src, SPREAD, SPREAD_LEN);

    nad_stack_drop(src);
    nad_stack_drop(dst);
}

static void test_copy_assign_self_is_noop() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    NAD_TEST_OK(nad_stack_copy_assign(s, s));

    assert_elems(s, SPREAD, SPREAD_LEN);

    nad_stack_drop(s);
}

/* ========== to span ========== */

// the span runs bottom to top, which is push order — the reverse of the order the elems
// will come off in
static void test_to_span_shows_bottom_to_top() {
    nad_Stack *s = make_stack(SPREAD, SPREAD_LEN);
    const nad_Span sp = nad_stack_to_span(s);

    TEST_ASSERT_EQUAL_size_t(SPREAD_LEN, sp.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), sp.elem_size);
    TEST_ASSERT_EQUAL_INT32_ARRAY(SPREAD, sp.data, SPREAD_LEN);

    nad_stack_drop(s);
}

// the top is the LAST elem of the span, not the first — that is the whole difference
// between this and a queue
static void test_to_span_ends_at_the_top() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const nad_Span sp = nad_stack_to_span(s);

    TEST_ASSERT_EQUAL_PTR(nad_span_get(sp, sp.len - 1), nad_stack_top(s));

    nad_stack_drop(s);
}

static void test_to_span_follows_a_pop() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    nad_stack_pop(s);
    nad_stack_pop(s);

    assert_elems(s, SPREAD, SPREAD_LEN - 2);

    nad_stack_drop(s);
}

static void test_to_span_of_empty_has_no_elems() {
    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW(int32_t, nad_al_default(), &s));

    const nad_Span sp = nad_stack_to_span(s);

    TEST_ASSERT_EQUAL_size_t(0, sp.len);
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), sp.elem_size);

    nad_stack_drop(s);
}

// the point of the one way bridge: algo reads the span, and the arrival order that gives
// the stack its meaning is not something algo can rearrange — there is no mutable view
static void test_the_span_reaches_algo_and_the_stack_is_untouched() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);
    const nad_Span sp = nad_stack_to_span(s);

    TEST_ASSERT_EQUAL_size_t(2, nad_span_max_elem(sp, nad_cmp_i32));
    TEST_ASSERT_FALSE(nad_span_is_sorted(sp, nad_cmp_i32));

    assert_elems(s, SPREAD, SPREAD_LEN);

    nad_stack_drop(s);
}

/* ========== swap ========== */

static void test_swap_exchanges_the_elems() {
    nad_Stack *a = nullptr;
    nad_Stack *b = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, nad_al_default(), &a, 1, 2, 3));
    NAD_TEST_OK(NAD_STACK_OF(int32_t, nad_al_default(), &b, 10, 20));

    NAD_TEST_OK(nad_stack_swap(a, b));

    constexpr int32_t want_a[2] = {10, 20};
    constexpr int32_t want_b[3] = {1, 2, 3};
    assert_elems(a, want_a, 2);
    assert_elems(b, want_b, 3);
    TEST_ASSERT_EQUAL_INT32(20, *NAD_STACK_TOP_AS(int32_t, a));
    TEST_ASSERT_EQUAL_INT32(3, *NAD_STACK_TOP_AS(int32_t, b));

    nad_stack_drop(a);
    nad_stack_drop(b);
}

static void test_swap_self_is_noop() {
    nad_Stack *s = make_stack_from(SPREAD, SPREAD_LEN);

    NAD_TEST_OK(nad_stack_swap(s, s));

    assert_elems(s, SPREAD, SPREAD_LEN);

    nad_stack_drop(s);
}

// two allocators: neither may free the other's memory, so the bytes move and each side
// keeps the allocator it was built with
static void test_swap_across_allocators_moves_the_bytes() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Stack *a = nullptr;
    nad_Stack *b = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, nad_al_default(), &a, 1, 2, 3));
    NAD_TEST_OK(NAD_STACK_OF(int32_t, arena, &b, 10, 20, 30));

    NAD_TEST_OK(nad_stack_swap(a, b));

    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_stack_al(a));
    TEST_ASSERT_EQUAL_PTR(arena, nad_stack_al(b));
    TEST_ASSERT_EQUAL_INT32(30, *NAD_STACK_TOP_AS(int32_t, a));
    TEST_ASSERT_EQUAL_INT32(3, *NAD_STACK_TOP_AS(int32_t, b));

    nad_stack_drop(a);
    nad_stack_drop(b);
    nad_al_arena_drop(arena);
}

/* ========== failures ========== */

static void test_new_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    nad_test_arena_leave(arena, 0);

    nad_Stack *s = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, NAD_STACK_NEW(int32_t, arena, &s));
    TEST_ASSERT_NULL(s);

    nad_al_arena_drop(arena);
}

static void test_from_data_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 128);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Stack *s = nullptr;
    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_STACK_FROM_DATA(int32_t, SPREAD, 1000, arena, &s)
    );
    TEST_ASSERT_NULL(s);

    nad_al_arena_drop(arena);
}

// a refused push must leave the stack exactly as it was — not one elem taller with a
// hole on top
static void test_push_reports_an_exhausted_arena_and_changes_nothing() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, arena, &s, 5, 1, 3));
    nad_test_arena_leave(arena, 0);

    constexpr int32_t val = 100;
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_stack_push(s, &val));

    constexpr int32_t want[3] = {5, 1, 3};
    assert_elems(s, want, 3);
    TEST_ASSERT_EQUAL_INT32(3, *NAD_STACK_TOP_AS(int32_t, s));

    nad_stack_drop(s);
    nad_al_arena_drop(arena);
}

static void test_copy_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Stack *src = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, arena, &src, 5, 1, 3));
    nad_test_arena_leave(arena, 0);

    nad_Stack *dst = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_stack_copy(src, &dst));
    TEST_ASSERT_NULL(dst);

    nad_stack_drop(src);
    nad_al_arena_drop(arena);
}

static void test_reserve_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_OF(int32_t, arena, &s, 5, 1, 3));
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_stack_reserve(s, 1000));

    constexpr int32_t want[3] = {5, 1, 3};
    assert_elems(s, want, 3);

    nad_stack_drop(s);
    nad_al_arena_drop(arena);
}

// The stack is built in two steps, the vec and then the header that hides it. When the
// second one is refused the first must not be stranded: the probe counts what is still
// live, and an arena would hide the leak because it frees everything at once
static void test_a_refused_header_frees_the_vec() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_test_probe_fail_after_next(&probe, 1);

    nad_Stack *s = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, NAD_STACK_NEW(int32_t, &al, &s));

    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the same, one allocation later: from_data takes a buffer as well
static void test_a_refused_header_frees_a_filled_vec() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_test_probe_fail_after_next(&probe, 2);

    nad_Stack *s = nullptr;
    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_STACK_FROM_DATA(int32_t, SPREAD, SPREAD_LEN, &al, &s)
    );

    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== compare ========== */

static void test_eq_matches_the_same_elems() {
    constexpr int32_t src[3] = {7, 8, 9};

    nad_Stack *a = make_stack(src, 3);
    nad_Stack *b = make_stack_from(src, 3);

    TEST_ASSERT_TRUE(nad_stack_eq(a, a));
    TEST_ASSERT_TRUE(nad_stack_eq(a, b));
    TEST_ASSERT_TRUE(nad_stack_eq(b, a));
    TEST_ASSERT_TRUE(nad_stack_eq_by(a, b, nad_eq_i32));

    nad_stack_drop(a);
    nad_stack_drop(b);
}

static void test_eq_parts_one_differing_elem() {
    constexpr int32_t lhs[3] = {7, 8, 9};
    constexpr int32_t rhs[3] = {7, 8, 99};

    nad_Stack *a = make_stack(lhs, 3);
    nad_Stack *b = make_stack(rhs, 3);

    TEST_ASSERT_FALSE(nad_stack_eq(a, b));
    TEST_ASSERT_FALSE(nad_stack_eq_by(a, b, nad_eq_i32));

    nad_stack_drop(a);
    nad_stack_drop(b);
}

static void test_eq_parts_different_lengths() {
    constexpr int32_t src[3] = {7, 8, 9};

    nad_Stack *a = make_stack(src, 3);
    nad_Stack *shorter = make_stack(src, 2);

    TEST_ASSERT_FALSE(nad_stack_eq(a, shorter));
    TEST_ASSERT_FALSE(nad_stack_eq(shorter, a));

    nad_stack_drop(a);
    nad_stack_drop(shorter);
}

static void test_eq_of_two_empties() {
    constexpr int32_t src[1] = {7};

    nad_Stack *a = make_stack(src, 0);
    nad_Stack *b = make_stack_from(src, 0);
    nad_Stack *one = make_stack(src, 1);

    TEST_ASSERT_TRUE(nad_stack_eq(a, b));
    TEST_ASSERT_TRUE(nad_stack_eq_by(a, b, nad_eq_i32));
    TEST_ASSERT_FALSE(nad_stack_eq(a, one));

    nad_stack_drop(a);
    nad_stack_drop(b);
    nad_stack_drop(one);
}

// a popped elem is gone even though its bytes are still in the vec underneath
static void test_eq_forgets_a_popped_elem() {
    constexpr int32_t lhs[2] = {7, 8};
    constexpr int32_t rhs[3] = {7, 8, 9};

    nad_Stack *a = make_stack(lhs, 2);
    nad_Stack *b = make_stack(rhs, 3);

    TEST_ASSERT_FALSE(nad_stack_eq(a, b));
    nad_stack_pop(b);
    TEST_ASSERT_TRUE(nad_stack_eq(a, b));

    nad_stack_drop(a);
    nad_stack_drop(b);
}

static void test_eq_by_asks_the_equality() {
    constexpr Pair lhs[2] = {{1, 10}, {2, 20}};
    constexpr Pair rhs[2] = {{1, 70}, {2, 80}};

    nad_Stack *a = nullptr;
    nad_Stack *b = nullptr;
    NAD_TEST_OK(NAD_STACK_FROM_DATA(Pair, lhs, 2, nad_al_default(), &a));
    NAD_TEST_OK(NAD_STACK_FROM_DATA(Pair, rhs, 2, nad_al_default(), &b));

    TEST_ASSERT_FALSE(nad_stack_eq(a, b));
    TEST_ASSERT_TRUE(nad_stack_eq_by(a, b, nad_test_pair_eq_a));

    nad_stack_drop(a);
    nad_stack_drop(b);
}

/* ========== into ========== */

// the vec was there all along: taking it copies nothing and leaves the elems where they
// were, bottom to top
static void test_into_vec_hands_the_elems_over() {
    constexpr int32_t src[3] = {7, 8, 9};
    nad_Stack *s = make_stack(src, 3);

    const void *before = nad_stack_to_span(s).data;
    const size_t cap = nad_stack_cap(s);

    nad_Vec *v = nad_stack_into_vec(s);

    TEST_ASSERT_EQUAL_PTR(before, nad_vec_data(v));
    TEST_ASSERT_EQUAL_size_t(3, nad_vec_len(v));
    TEST_ASSERT_EQUAL_size_t(cap, nad_vec_cap(v));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_vec_al(v));

    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_INT32(src[i], *NAD_VEC_GET_AS(int32_t, v, i));
    }

    nad_vec_drop(v);
}

static void test_into_vec_of_an_empty_stack() {
    constexpr int32_t src[1] = {7};
    nad_Stack *s = make_stack(src, 0);

    nad_Vec *v = nad_stack_into_vec(s);

    TEST_ASSERT_EQUAL_size_t(0, nad_vec_len(v));

    nad_vec_drop(v);
}

// only the adapter's own header goes back; dropping the vec afterwards squares the books
static void test_into_vec_releases_the_header_alone() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_Stack *s = nullptr;
    NAD_TEST_OK(NAD_STACK_NEW(int32_t, &al, &s));

    // the last block the constructor took is the adapter's own header, so this is what
    // into has to hand back, and with the size it was taken as
    const size_t header = probe.last_alloc_size;

    NAD_TEST_OK(NAD_STACK_PUSH(int32_t, s, 1));
    const size_t live = probe.live;

    nad_Vec *v = nad_stack_into_vec(s);

    TEST_ASSERT_EQUAL_size_t(live - 1, probe.live);
    TEST_ASSERT_EQUAL_size_t(header, probe.last_dealloc_size);

    nad_vec_drop(v);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
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
    RUN_TEST(test_swap_across_allocators_moves_the_bytes);

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

    return UNITY_END();
}
