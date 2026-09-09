#include "terse/ds/list.h"
#include "terse/algo/search.h"
#include "terse/algo/sort.h"
#include "terse/alloc/arena.h"
#include "terse/alloc/default.h"
#include "terse/core/cmp.h"
#include "terse/core/print.h"
#include "terse/core/util.h"

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

// an equality that sees less than the bytes do, to show that find asks for one rather
// than comparing the elems itself
static bool eq_abs_i32(const void *lhs, const void *rhs) {
    const int32_t a = *(const int32_t *) lhs;
    const int32_t b = *(const int32_t *) rhs;

    return (a < 0 ? -a : a) == (b < 0 ? -b : b);
}

static void push_int(trs_List *l, int32_t val) {
    TRS_TEST_OK(trs_list_push_back(l, &val));
}

// int32_t list holding 0, 1, ... len-1
static trs_List *make_list(size_t len) {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_NEW(int32_t, trs_al_default(), &l));

    for (size_t i = 0; i < len; ++i) {
        push_int(l, (int32_t) i);
    }
    return l;
}

// the whole contents, checked in BOTH directions: a broken next/prev pair
// leaves one of the two walks intact, so a one-way check misses half the bugs
static void assert_elems(const trs_List *l, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_list_len(l));

    size_t seen = 0;
    for (const trs_ListNode *node = trs_list_front_node(l); node; node = trs_list_node_next(node)) {
        TEST_ASSERT_TRUE_MESSAGE(seen < n, "forward walk is longer than expected");
        TEST_ASSERT_EQUAL_INT32(want[seen], *TRS_LIST_NODE_ELEM_AS(int32_t, node));
        ++seen;
    }
    TEST_ASSERT_EQUAL_size_t(n, seen);

    seen = 0;
    for (const trs_ListNode *node = trs_list_back_node(l); node; node = trs_list_node_prev(node)) {
        TEST_ASSERT_TRUE_MESSAGE(seen < n, "backward walk is longer than expected");
        TEST_ASSERT_EQUAL_INT32(want[n - seen - 1], *TRS_LIST_NODE_ELEM_AS(int32_t, node));
        ++seen;
    }
    TEST_ASSERT_EQUAL_size_t(n, seen);

    if (n > 0) {
        TEST_ASSERT_EQUAL_INT32(want[0], *TRS_LIST_FRONT_AS(int32_t, l));
        TEST_ASSERT_EQUAL_INT32(want[n - 1], *TRS_LIST_BACK_AS(int32_t, l));
    } else {
        TEST_ASSERT_NULL(trs_list_front_node(l));
        TEST_ASSERT_NULL(trs_list_back_node(l));
    }
}

// order over Pair by its first field alone, so the second is free to witness stability
static int cmp_pair_a(const void *lhs, const void *rhs) {
    return trs_cmp_i64(&((const Pair *) lhs)->a, &((const Pair *) rhs)->a);
}

[[nodiscard]]
static trs_ListNode *node_at(trs_List *l, size_t idx) {
    trs_ListNode *node = trs_list_front_node_mut(l);
    for (size_t i = 0; i < idx; ++i) {
        node = trs_list_node_next_mut(node);
    }
    TEST_ASSERT_NOT_NULL(node);
    return node;
}

// the node addresses, front to back — what a relinking operation must preserve and a
// copying one cannot
static void collect_nodes(const trs_List *l, const trs_ListNode **dst, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_list_len(l));

    size_t i = 0;
    for (const trs_ListNode *node = trs_list_front_node(l); node; node = trs_list_node_next(node)) {
        dst[i++] = node;
    }
}

// every node of 'want' is still a node of 'l', in whatever order
static void assert_same_nodes(const trs_List *l, const trs_ListNode **want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, trs_list_len(l));

    for (size_t i = 0; i < n; ++i) {
        bool found = false;
        for (const trs_ListNode *node = trs_list_front_node(l); node; node = trs_list_node_next(node)) {
            if (node == want[i]) {
                found = true;
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(found, "a node was replaced instead of relinked");
    }
}

static void assert_empty(const trs_List *l) {
    assert_elems(l, nullptr, 0);
}

/* ========== lifetime ========== */

static void test_new_starts_empty() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_NEW(int32_t, trs_al_default(), &l));

    assert_empty(l);

    trs_list_drop(l);
}

static void test_new_keeps_elem_size_and_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_NEW(Pair, arena, &l));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), trs_list_elem_size(l));
    TEST_ASSERT_EQUAL_PTR(arena, trs_list_al(l));

    trs_list_drop(l);
    trs_al_arena_drop(arena);
}

static void test_from_data_copies_and_detaches_the_source() {
    int32_t src[] = {1, 2, 3};
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_FROM_DATA(int32_t, src, 3, trs_al_default(), &l));

    src[0] = 99; // the list must not follow the source

    assert_elems(l, (int32_t[]){1, 2, 3}, 3);

    trs_list_drop(l);
}

static void test_from_data_empty_is_empty() {
    trs_List *l = nullptr;
    TRS_TEST_OK(trs_list_from_data(nullptr, 0, sizeof(int32_t), trs_al_default(), &l));

    assert_empty(l);

    trs_list_drop(l);
}

// a wide elem must travel whole, not truncated to a word
static void test_from_data_copies_whole_elems() {
    constexpr Pair src[] = {{1, 2}, {3, 4}};
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_FROM_DATA(Pair, src, 2, trs_al_default(), &l));

    const Pair *first = TRS_LIST_FRONT_AS(Pair, l);
    const Pair *last = TRS_LIST_BACK_AS(Pair, l);

    TEST_ASSERT_EQUAL_INT64(1, first->a);
    TEST_ASSERT_EQUAL_INT64(2, first->b);
    TEST_ASSERT_EQUAL_INT64(3, last->a);
    TEST_ASSERT_EQUAL_INT64(4, last->b);

    trs_list_drop(l);
}

static void test_from_span_copies_the_view() {
    constexpr int32_t src[] = {5, 6, 7};
    trs_List *l = nullptr;
    TRS_TEST_OK(trs_list_from_span(TRS_SPAN_FROM_DATA(int32_t, src, 3), trs_al_default(), &l));

    assert_elems(l, (int32_t[]){5, 6, 7}, 3);

    trs_list_drop(l);
}

static void test_drop_null_is_noop() {
    trs_list_drop(nullptr);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    trs_List *l = make_list(3);
    trs_List *c = nullptr;
    TRS_TEST_OK(trs_list_copy(l, &c));

    trs_list_pop_back(c);
    push_int(l, 9);

    assert_elems(l, (int32_t[]){0, 1, 2, 9}, 4);
    assert_elems(c, (int32_t[]){0, 1}, 2);

    trs_list_drop(c);
    trs_list_drop(l);
}

static void test_copy_inherits_the_source_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &l, 1, 2));

    trs_List *c = nullptr;
    TRS_TEST_OK(trs_list_copy(l, &c));

    TEST_ASSERT_EQUAL_PTR(arena, trs_list_al(c));

    trs_al_arena_drop(arena);
}

static void test_copy_with_builds_on_the_given_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *src = make_list(4);

    trs_List *dst = nullptr;
    TRS_TEST_OK(trs_list_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_list_al(dst));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_list_al(src));
    TEST_ASSERT_TRUE(trs_list_eq(src, dst));

    // the source is gone and the copy still holds the elems: every node is its own
    trs_list_drop(src);
    assert_elems(dst, (int32_t[]){0, 1, 2, 3}, 4);

    trs_list_drop(dst);
    trs_al_arena_drop(arena);
}

// the nodes are asked of the allocator the copy is going to, not of the source's
static void test_copy_with_reports_an_exhausted_target_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_List *src = make_list(4);

    trs_List *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_list_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(4, trs_list_len(src));

    trs_list_drop(src);
    trs_al_arena_drop(arena);
}

static void test_move_assign_hands_over_the_contents_on_one_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_List *src = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &src, 1, 2, 3));

    trs_List *dst = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &dst, 9));

    // a position borrowed from the source before the move: on one allocator the nodes are
    // relinked where they lie, so it must still be good afterwards — and belong to 'dst'
    const trs_ListNode *node = trs_list_front_node(src);

    const size_t requests = trs_test_probe_requests(&probe);
    TRS_TEST_OK(trs_list_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    assert_elems(dst, (int32_t[]){1, 2, 3}, 3);
    TEST_ASSERT_EQUAL_PTR(node, trs_list_front_node(dst));
    TEST_ASSERT_EQUAL_INT32(1, *TRS_LIST_NODE_ELEM_AS(int32_t, node));

    TEST_ASSERT_EQUAL_size_t(0, trs_list_len(src));
    TEST_ASSERT_NULL(trs_list_front_node(src));

    trs_list_drop(src);
    trs_list_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_move_assign_across_allocators_empties_the_source() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *src = make_list(4);

    trs_List *dst = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &dst, 9));

    TRS_TEST_OK(trs_list_move_assign(src, dst));

    assert_elems(dst, (int32_t[]){0, 1, 2, 3}, 4);
    TEST_ASSERT_EQUAL_PTR(arena, trs_list_al(dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_list_len(src));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_list_al(src));

    trs_list_drop(src);
    trs_list_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_across_allocators_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *dst = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &dst, 9));
    trs_test_arena_leave(arena, 0);

    trs_List *src = make_list(4);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_list_move_assign(src, dst));

    assert_elems(src, (int32_t[]){0, 1, 2, 3}, 4);
    assert_elems(dst, (int32_t[]){9}, 1);

    trs_list_drop(src);
    trs_list_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_of_itself_changes_nothing() {
    trs_List *l = make_list(3);

    TRS_TEST_OK(trs_list_move_assign(l, l));

    assert_elems(l, (int32_t[]){0, 1, 2}, 3);

    trs_list_drop(l);
}

static void test_copy_of_empty_is_empty() {
    trs_List *l = make_list(0);
    trs_List *c = nullptr;
    TRS_TEST_OK(trs_list_copy(l, &c));

    assert_empty(c);

    trs_list_drop(c);
    trs_list_drop(l);
}

static void test_copy_assign_grows_the_target() {
    trs_List *src = make_list(4);
    trs_List *dst = make_list(1);

    TRS_TEST_OK(trs_list_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){0, 1, 2, 3}, 4);
    assert_elems(src, (int32_t[]){0, 1, 2, 3}, 4);

    trs_list_drop(dst);
    trs_list_drop(src);
}

static void test_copy_assign_shrinks_the_target() {
    trs_List *src = make_list(1);
    trs_List *dst = make_list(4);

    TRS_TEST_OK(trs_list_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){0}, 1);

    trs_list_drop(dst);
    trs_list_drop(src);
}

static void test_copy_assign_from_empty_empties_the_target() {
    trs_List *src = make_list(0);
    trs_List *dst = make_list(3);

    TRS_TEST_OK(trs_list_copy_assign(src, dst));

    assert_empty(dst);
    push_int(dst, 7); // still usable
    assert_elems(dst, (int32_t[]){7}, 1);

    trs_list_drop(dst);
    trs_list_drop(src);
}

static void test_copy_assign_to_empty_target() {
    trs_List *src = make_list(2);
    trs_List *dst = make_list(0);

    TRS_TEST_OK(trs_list_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){0, 1}, 2);

    trs_list_drop(dst);
    trs_list_drop(src);
}

static void test_copy_assign_self_is_noop() {
    trs_List *l = make_list(3);

    TRS_TEST_OK(trs_list_copy_assign(l, l));

    assert_elems(l, (int32_t[]){0, 1, 2}, 3);

    trs_list_drop(l);
}

static void test_copy_assign_keeps_the_target_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *src = make_list(2);
    trs_List *dst = nullptr;
    TRS_TEST_OK(TRS_LIST_NEW(int32_t, arena, &dst));

    TRS_TEST_OK(trs_list_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_list_al(dst));
    assert_elems(dst, (int32_t[]){0, 1}, 2);

    trs_list_drop(src);
    trs_al_arena_drop(arena);
}

// the shared prefix keeps its nodes, so assignment costs allocations
// only for the elems the target had no node for
static void test_copy_assign_reuses_the_target_nodes() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_bare(&probe);

    trs_List *src = make_list(2);
    trs_List *dst = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &dst, 7, 8));

    const trs_ListNode *head = trs_list_front_node(dst);
    const trs_ListNode *tail = trs_list_back_node(dst);
    const size_t before = trs_test_probe_requests(&probe);

    TRS_TEST_OK(trs_list_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(before, trs_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_PTR(head, trs_list_front_node(dst));
    TEST_ASSERT_EQUAL_PTR(tail, trs_list_back_node(dst));
    assert_elems(dst, (int32_t[]){0, 1}, 2);

    trs_list_drop(dst);
    trs_list_drop(src);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== info ========== */

static void test_len_counts_the_elems() {
    trs_List *l = make_list(0);
    TEST_ASSERT_EQUAL_size_t(0, trs_list_len(l));

    push_int(l, 1);
    TEST_ASSERT_EQUAL_size_t(1, trs_list_len(l));

    push_int(l, 2);
    TEST_ASSERT_EQUAL_size_t(2, trs_list_len(l));

    trs_list_pop_front(l);
    TEST_ASSERT_EQUAL_size_t(1, trs_list_len(l));

    trs_list_drop(l);
}

static void test_elem_size_is_the_ctor_arg() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_NEW(Pair, trs_al_default(), &l));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), trs_list_elem_size(l));

    trs_list_drop(l);
}

static void test_al_is_the_ctor_arg() {
    trs_List *l = make_list(0);

    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_list_al(l));

    trs_list_drop(l);
}

/* ========== access ========== */

static void test_first_and_last_see_both_ends() {
    trs_List *l = make_list(3);

    TEST_ASSERT_EQUAL_INT32(0, *TRS_LIST_FRONT_AS(int32_t, l));
    TEST_ASSERT_EQUAL_INT32(2, *TRS_LIST_BACK_AS(int32_t, l));

    trs_list_drop(l);
}

static void test_first_and_last_meet_on_a_single_elem() {
    trs_List *l = make_list(1);

    TEST_ASSERT_EQUAL_PTR(trs_list_front(l), trs_list_back(l));

    trs_list_drop(l);
}

static void test_first_mut_writes_through() {
    trs_List *l = make_list(3);

    *TRS_LIST_FRONT_MUT_AS(int32_t, l) = 9;

    assert_elems(l, (int32_t[]){9, 1, 2}, 3);

    trs_list_drop(l);
}

static void test_last_mut_writes_through() {
    trs_List *l = make_list(3);

    *TRS_LIST_BACK_MUT_AS(int32_t, l) = 9;

    assert_elems(l, (int32_t[]){0, 1, 9}, 3);

    trs_list_drop(l);
}

/* ========== nodes ========== */

static void test_nodes_of_an_empty_list_are_null() {
    trs_List *l = make_list(0);

    TEST_ASSERT_NULL(trs_list_front_node(l));
    TEST_ASSERT_NULL(trs_list_back_node(l));

    trs_list_drop(l);
}

static void test_the_ends_are_the_same_node_on_a_single_elem() {
    trs_List *l = make_list(1);

    TEST_ASSERT_EQUAL_PTR(trs_list_front_node(l), trs_list_back_node(l));
    TEST_ASSERT_NULL(trs_list_node_next(trs_list_front_node(l)));
    TEST_ASSERT_NULL(trs_list_node_prev(trs_list_front_node(l)));

    trs_list_drop(l);
}

// the backward walk has a mutable twin: one pass from the back writes through every elem
static void test_node_prev_mut_writes_through_the_backward_walk() {
    trs_List *l = make_list(4);

    size_t seen = 0;
    for (trs_ListNode *node = trs_list_back_node_mut(l); node; node = trs_list_node_prev_mut(node)) {
        *TRS_LIST_NODE_ELEM_MUT_AS(int32_t, node) *= 10;
        ++seen;
    }
    TEST_ASSERT_EQUAL_size_t(4, seen);

    constexpr int32_t want[4] = {0, 10, 20, 30};
    assert_elems(l, want, 4);

    trs_list_drop(l);
}

static void test_next_and_prev_are_each_others_inverse() {
    trs_List *l = make_list(3);

    const trs_ListNode *head = trs_list_front_node(l);
    const trs_ListNode *mid = trs_list_node_next(head);
    const trs_ListNode *tail = trs_list_node_next(mid);

    TEST_ASSERT_EQUAL_PTR(tail, trs_list_back_node(l));
    TEST_ASSERT_NULL(trs_list_node_next(tail));
    TEST_ASSERT_EQUAL_PTR(mid, trs_list_node_prev(tail));
    TEST_ASSERT_EQUAL_PTR(head, trs_list_node_prev(mid));
    TEST_ASSERT_NULL(trs_list_node_prev(head));

    trs_list_drop(l);
}

static void test_node_elem_mut_writes_through() {
    trs_List *l = make_list(3);

    trs_ListNode *mid = trs_list_node_next_mut(trs_list_front_node_mut(l));
    *TRS_LIST_NODE_ELEM_MUT_AS(int32_t, mid) = 9;

    assert_elems(l, (int32_t[]){0, 9, 2}, 3);

    trs_list_drop(l);
}

/* ========== push / pop ========== */

static void test_push_back_appends() {
    trs_List *l = make_list(0);

    push_int(l, 1);
    push_int(l, 2);

    assert_elems(l, (int32_t[]){1, 2}, 2);

    trs_list_drop(l);
}

static void test_push_front_prepends() {
    trs_List *l = make_list(0);
    int32_t one = 1;
    int32_t two = 2;

    TRS_TEST_OK(trs_list_push_front(l, &one));
    TRS_TEST_OK(trs_list_push_front(l, &two));

    assert_elems(l, (int32_t[]){2, 1}, 2);

    trs_list_drop(l);
}

// both ends grow at once: the two chains must stay consistent
static void test_push_front_and_back_interleave() {
    trs_List *l = make_list(0);

    TRS_TEST_OK(TRS_LIST_PUSH_BACK(int32_t, l, 1));
    TRS_TEST_OK(TRS_LIST_PUSH_FRONT(int32_t, l, 0));
    TRS_TEST_OK(TRS_LIST_PUSH_BACK(int32_t, l, 2));
    TRS_TEST_OK(TRS_LIST_PUSH_FRONT(int32_t, l, -1));

    assert_elems(l, (int32_t[]){-1, 0, 1, 2}, 4);

    trs_list_drop(l);
}

static void test_pop_front_drops_the_head() {
    trs_List *l = make_list(3);

    trs_list_pop_front(l);

    assert_elems(l, (int32_t[]){1, 2}, 2);

    trs_list_drop(l);
}

static void test_pop_back_drops_the_tail() {
    trs_List *l = make_list(3);

    trs_list_pop_back(l);

    assert_elems(l, (int32_t[]){0, 1}, 2);

    trs_list_drop(l);
}

static void test_popping_to_empty_resets_both_ends() {
    trs_List *l = make_list(2);

    trs_list_pop_front(l);
    trs_list_pop_back(l);

    assert_empty(l);

    trs_list_drop(l);
}

static void test_push_after_emptying_works() {
    trs_List *l = make_list(2);

    trs_list_pop_back(l);
    trs_list_pop_back(l);
    push_int(l, 7);

    assert_elems(l, (int32_t[]){7}, 1);

    trs_list_drop(l);
}

/* ========== insert / remove ========== */

static void test_insert_before_the_middle() {
    trs_List *l = make_list(3);
    trs_ListNode *mid = trs_list_node_next_mut(trs_list_front_node_mut(l));

    TRS_TEST_OK(TRS_LIST_INSERT_BEFORE(int32_t, l, mid, 9));

    assert_elems(l, (int32_t[]){0, 9, 1, 2}, 4);

    trs_list_drop(l);
}

static void test_insert_after_the_middle() {
    trs_List *l = make_list(3);
    trs_ListNode *mid = trs_list_node_next_mut(trs_list_front_node_mut(l));

    TRS_TEST_OK(TRS_LIST_INSERT_AFTER(int32_t, l, mid, 9));

    assert_elems(l, (int32_t[]){0, 1, 9, 2}, 4);

    trs_list_drop(l);
}

static void test_insert_before_the_head_moves_the_head() {
    trs_List *l = make_list(2);

    TRS_TEST_OK(TRS_LIST_INSERT_BEFORE(int32_t, l, trs_list_front_node_mut(l), 9));

    assert_elems(l, (int32_t[]){9, 0, 1}, 3);

    trs_list_drop(l);
}

static void test_insert_after_the_tail_moves_the_tail() {
    trs_List *l = make_list(2);

    TRS_TEST_OK(TRS_LIST_INSERT_AFTER(int32_t, l, trs_list_back_node_mut(l), 9));

    assert_elems(l, (int32_t[]){0, 1, 9}, 3);

    trs_list_drop(l);
}

// the header promises where the new node lands
static void test_the_new_node_is_reachable_from_the_anchor() {
    trs_List *l = make_list(3);
    trs_ListNode *mid = trs_list_node_next_mut(trs_list_front_node_mut(l));

    TRS_TEST_OK(TRS_LIST_INSERT_BEFORE(int32_t, l, mid, 7));
    TRS_TEST_OK(TRS_LIST_INSERT_AFTER(int32_t, l, mid, 8));

    TEST_ASSERT_EQUAL_INT32(7, *TRS_LIST_NODE_ELEM_AS(int32_t, trs_list_node_prev(mid)));
    TEST_ASSERT_EQUAL_INT32(8, *TRS_LIST_NODE_ELEM_AS(int32_t, trs_list_node_next(mid)));

    trs_list_drop(l);
}

static void test_insert_into_a_single_elem_list() {
    trs_List *l = make_list(1);
    trs_ListNode *only = trs_list_front_node_mut(l);

    TRS_TEST_OK(TRS_LIST_INSERT_BEFORE(int32_t, l, only, -1));
    TRS_TEST_OK(TRS_LIST_INSERT_AFTER(int32_t, l, only, 1));

    assert_elems(l, (int32_t[]){-1, 0, 1}, 3);

    trs_list_drop(l);
}

static void test_remove_from_the_middle() {
    trs_List *l = make_list(3);
    trs_ListNode *mid = trs_list_node_next_mut(trs_list_front_node_mut(l));

    trs_list_remove(l, mid);

    assert_elems(l, (int32_t[]){0, 2}, 2);

    trs_list_drop(l);
}

static void test_remove_the_head() {
    trs_List *l = make_list(3);

    trs_list_remove(l, trs_list_front_node_mut(l));

    assert_elems(l, (int32_t[]){1, 2}, 2);

    trs_list_drop(l);
}

static void test_remove_the_tail() {
    trs_List *l = make_list(3);

    trs_list_remove(l, trs_list_back_node_mut(l));

    assert_elems(l, (int32_t[]){0, 1}, 2);

    trs_list_drop(l);
}

static void test_remove_the_only_node_empties_the_list() {
    trs_List *l = make_list(1);

    trs_list_remove(l, trs_list_front_node_mut(l));

    assert_empty(l);

    trs_list_drop(l);
}

/* ========== clear ========== */

static void test_clear_empties_the_list() {
    trs_List *l = make_list(3);

    trs_list_clear(l);

    assert_empty(l);

    trs_list_drop(l);
}

static void test_clear_of_an_empty_list_is_noop() {
    trs_List *l = make_list(0);

    trs_list_clear(l);
    trs_list_clear(l);

    assert_empty(l);

    trs_list_drop(l);
}

static void test_clear_leaves_the_list_usable() {
    trs_List *l = make_list(3);

    trs_list_clear(l);
    push_int(l, 7);
    TRS_TEST_OK(TRS_LIST_PUSH_FRONT(int32_t, l, 6));

    assert_elems(l, (int32_t[]){6, 7}, 2);

    trs_list_drop(l);
}

static void test_clear_frees_every_node() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_bare(&probe);

    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &l, 1, 2, 3));

    trs_list_clear(l);

    TEST_ASSERT_EQUAL_size_t(1, probe.live); // the list struct itself

    trs_list_drop(l);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== splice ========== */

static void test_splice_back_appends_and_empties_the_source() {
    trs_List *dst = make_list(2);
    trs_List *src = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &src, 7, 8));

    TRS_TEST_OK(trs_list_splice_back(dst, src));

    assert_elems(dst, (int32_t[]){0, 1, 7, 8}, 4);
    assert_empty(src);

    trs_list_drop(src);
    trs_list_drop(dst);
}

static void test_splice_front_prepends_and_empties_the_source() {
    trs_List *dst = make_list(2);
    trs_List *src = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &src, 7, 8));

    TRS_TEST_OK(trs_list_splice_front(dst, src));

    assert_elems(dst, (int32_t[]){7, 8, 0, 1}, 4);
    assert_empty(src);

    trs_list_drop(src);
    trs_list_drop(dst);
}

static void test_splice_into_an_empty_list() {
    trs_List *dst = make_list(0);
    trs_List *src = make_list(3);

    TRS_TEST_OK(trs_list_splice_back(dst, src));

    assert_elems(dst, (int32_t[]){0, 1, 2}, 3);
    assert_empty(src);

    trs_list_drop(src);
    trs_list_drop(dst);
}

static void test_splice_front_into_an_empty_list() {
    trs_List *dst = make_list(0);
    trs_List *src = make_list(2);

    TRS_TEST_OK(trs_list_splice_front(dst, src));

    assert_elems(dst, (int32_t[]){0, 1}, 2);
    assert_empty(src);

    trs_list_drop(src);
    trs_list_drop(dst);
}

static void test_splice_from_an_empty_list_is_noop() {
    trs_List *dst = make_list(2);
    trs_List *src = make_list(0);

    TRS_TEST_OK(trs_list_splice_back(dst, src));
    TRS_TEST_OK(trs_list_splice_front(dst, src));

    assert_elems(dst, (int32_t[]){0, 1}, 2);
    assert_empty(src);

    trs_list_drop(src);
    trs_list_drop(dst);
}

// sharing an allocator makes splice a pointer rewire: no allocation,
// and the very same nodes end up in the target
static void test_splice_moves_the_nodes_without_allocating() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_bare(&probe);

    trs_List *dst = nullptr;
    trs_List *src = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &dst, 1, 2));
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &src, 3, 4));

    const trs_ListNode *src_head = trs_list_front_node(src);
    const size_t before = trs_test_probe_requests(&probe);

    TRS_TEST_OK(trs_list_splice_back(dst, src));

    TEST_ASSERT_EQUAL_size_t(before, trs_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_PTR(src_head, trs_list_node_next(trs_list_node_next(trs_list_front_node(dst))));
    assert_elems(dst, (int32_t[]){1, 2, 3, 4}, 4);

    trs_list_drop(src);
    trs_list_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// across allocators the nodes cannot travel, so the elems are copied
// into the target's allocator and the source is emptied all the same
static void test_splice_across_allocators_copies_the_elems() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_bare(&probe);

    trs_List *dst = make_list(2);
    trs_List *src = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &src, 7, 8));

    TRS_TEST_OK(trs_list_splice_back(dst, src));

    assert_elems(dst, (int32_t[]){0, 1, 7, 8}, 4);
    assert_empty(src);
    TEST_ASSERT_EQUAL_size_t(1, probe.live); // src kept nothing but its own struct

    trs_list_drop(src);
    trs_list_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== swap ========== */

static void test_find_returns_the_first_node_holding_the_elem() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 1, 2, 3, 2));

    const trs_ListNode *node = TRS_LIST_FIND(int32_t, l, 2, trs_eq_i32);
    TEST_ASSERT_NOT_NULL(node);

    // the first of the two, not just any: the one whose next holds 3
    TEST_ASSERT_EQUAL_INT32(3, *TRS_LIST_NODE_ELEM_AS(int32_t, trs_list_node_next(node)));

    trs_list_drop(l);
}

static void test_find_of_a_missing_elem_is_null() {
    trs_List *l = make_list(4);

    TEST_ASSERT_NULL(TRS_LIST_FIND(int32_t, l, 99, trs_eq_i32));

    trs_list_drop(l);
}

static void test_find_of_an_empty_list_is_null() {
    trs_List *l = make_list(0);

    TEST_ASSERT_NULL(TRS_LIST_FIND(int32_t, l, 0, trs_eq_i32));

    trs_list_drop(l);
}

// what the mut door is for: the position it hands back is what insert and remove take
static void test_find_mut_gives_a_position_to_write_through() {
    trs_List *l = make_list(4);

    trs_ListNode *at = TRS_LIST_FIND_MUT(int32_t, l, 2, trs_eq_i32);
    TEST_ASSERT_NOT_NULL(at);

    *TRS_LIST_NODE_ELEM_MUT_AS(int32_t, at) = 20;
    TRS_TEST_OK(TRS_LIST_INSERT_BEFORE(int32_t, l, at, 9));

    assert_elems(l, (int32_t[]){0, 1, 9, 20, 3}, 5);

    trs_list_remove(l, at);
    assert_elems(l, (int32_t[]){0, 1, 9, 3}, 4);

    trs_list_drop(l);
}

// the equality is the caller's, so find answers by whatever it means by equal
static void test_find_asks_the_equality_it_is_given() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 1, -2, 3));

    TEST_ASSERT_NULL(TRS_LIST_FIND(int32_t, l, 2, trs_eq_i32));
    TEST_ASSERT_NOT_NULL(TRS_LIST_FIND(int32_t, l, 2, eq_abs_i32));

    trs_list_drop(l);
}

static void test_for_each_walks_front_to_back() {
    trs_List *l = make_list(4);

    int32_t seen[4];
    size_t n = 0;
    TRS_LIST_FOR_EACH(node, l) {
        seen[n++] = *TRS_LIST_NODE_ELEM_AS(int32_t, node);
    }

    TEST_ASSERT_EQUAL_size_t(4, n);
    TEST_ASSERT_EQUAL_INT32_ARRAY(((int32_t[]){0, 1, 2, 3}), seen, 4);

    trs_list_drop(l);
}

static void test_for_each_over_an_empty_list_runs_no_body() {
    trs_List *l = make_list(0);

    size_t n = 0;
    TRS_LIST_FOR_EACH(node, l) {
        TRS_UNUSED(node);
        ++n;
    }

    TEST_ASSERT_EQUAL_size_t(0, n);

    trs_list_drop(l);
}

static void test_for_each_mut_writes_through_every_position() {
    trs_List *l = make_list(4);

    TRS_LIST_FOR_EACH_MUT(node, l) {
        *TRS_LIST_NODE_ELEM_MUT_AS(int32_t, node) *= 10;
    }

    assert_elems(l, (int32_t[]){0, 10, 20, 30}, 4);

    trs_list_drop(l);
}

static void test_swap_exchanges_the_contents() {
    trs_List *a = make_list(3);
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &b, 9));

    trs_list_swap(a, b);

    assert_elems(a, (int32_t[]){9}, 1);
    assert_elems(b, (int32_t[]){0, 1, 2}, 3);

    trs_list_drop(b);
    trs_list_drop(a);
}

// the nodes do not move, they only change list — that is what makes
// swap O(1) and every borrowed node still valid afterwards
static void test_swap_keeps_the_nodes_alive() {
    trs_List *a = make_list(2);
    trs_List *b = make_list(1);

    const trs_ListNode *a_head = trs_list_front_node(a);
    const trs_ListNode *b_head = trs_list_front_node(b);

    trs_list_swap(a, b);

    TEST_ASSERT_EQUAL_PTR(a_head, trs_list_front_node(b));
    TEST_ASSERT_EQUAL_PTR(b_head, trs_list_front_node(a));

    trs_list_drop(b);
    trs_list_drop(a);
}

static void test_swap_self_is_noop() {
    trs_List *l = make_list(3);

    trs_list_swap(l, l);

    assert_elems(l, (int32_t[]){0, 1, 2}, 3);

    trs_list_drop(l);
}

static void test_swap_with_an_empty_list_works_both_ways() {
    trs_List *l = make_list(2);
    trs_List *none = make_list(0);

    trs_list_swap(l, none);
    assert_empty(l);
    assert_elems(none, (int32_t[]){0, 1}, 2);

    trs_list_swap(l, none);
    assert_elems(l, (int32_t[]){0, 1}, 2);
    assert_empty(none);

    push_int(none, 7); // both still usable
    assert_elems(none, (int32_t[]){7}, 1);

    trs_list_drop(none);
    trs_list_drop(l);
}

/* ========== splice_node ========== */

static void test_splice_node_moves_one_node_between_lists() {
    trs_List *a = make_list(3); // 0, 1, 2
    trs_List *b = make_list(2); // 0, 1

    TRS_TEST_OK(trs_list_splice_node(b, node_at(b, 1), a, node_at(a, 1)));

    constexpr int32_t want_a[2] = {0, 2};
    constexpr int32_t want_b[3] = {0, 1, 1};
    assert_elems(a, want_a, 2);
    assert_elems(b, want_b, 3);

    trs_list_drop(a);
    trs_list_drop(b);
}

// the whole reason the operation exists: the node itself changes lists, so a pointer
// held before the move still names the same node afterwards
static void test_splice_node_keeps_the_node_address() {
    trs_List *a = make_list(3);
    trs_List *b = make_list(1);

    const trs_ListNode *moved = node_at(a, 2);
    TRS_TEST_OK(trs_list_splice_node(b, nullptr, a, node_at(a, 2)));

    TEST_ASSERT_EQUAL_PTR(moved, trs_list_back_node(b));
    TEST_ASSERT_EQUAL_INT32(2, *TRS_LIST_NODE_ELEM_AS(int32_t, moved));

    trs_list_drop(a);
    trs_list_drop(b);
}

static void test_splice_node_at_null_appends_to_the_back() {
    trs_List *a = make_list(2); // 0, 1
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &b, 7, 8));

    TRS_TEST_OK(trs_list_splice_node(b, nullptr, a, node_at(a, 0)));

    constexpr int32_t want_a[1] = {1};
    constexpr int32_t want_b[3] = {7, 8, 0};
    assert_elems(a, want_a, 1);
    assert_elems(b, want_b, 3);

    trs_list_drop(a);
    trs_list_drop(b);
}

// one list on both sides: the node is unlinked before its new neighbour is read, which
// is what keeps a move inside one list from landing next to itself
static void test_splice_node_within_one_list_moves_the_elem() {
    trs_List *l = make_list(4); // 0, 1, 2, 3

    TRS_TEST_OK(trs_list_splice_node(l, node_at(l, 1), l, node_at(l, 3)));

    constexpr int32_t want[4] = {0, 3, 1, 2};
    assert_elems(l, want, 4);

    trs_list_drop(l);
}

static void test_splice_node_to_the_front_of_the_same_list() {
    trs_List *l = make_list(3); // 0, 1, 2

    TRS_TEST_OK(trs_list_splice_node(l, node_at(l, 0), l, node_at(l, 2)));

    constexpr int32_t want[3] = {2, 0, 1};
    assert_elems(l, want, 3);

    trs_list_drop(l);
}

// a move one step forward inside one list: the naive order — read the neighbour, then
// unlink — puts the node back where it started
static void test_splice_node_one_step_forward() {
    trs_List *l = make_list(3); // 0, 1, 2

    TRS_TEST_OK(trs_list_splice_node(l, node_at(l, 2), l, node_at(l, 1)));

    constexpr int32_t want[3] = {0, 1, 2};
    assert_elems(l, want, 3);

    trs_list_drop(l);
}

static void test_splice_node_of_the_only_node_empties_the_source() {
    trs_List *a = make_list(1);
    trs_List *b = make_list(2);

    TRS_TEST_OK(trs_list_splice_node(b, trs_list_front_node_mut(b), a, node_at(a, 0)));

    assert_empty(a);
    constexpr int32_t want_b[3] = {0, 0, 1};
    assert_elems(b, want_b, 3);

    trs_list_drop(a);
    trs_list_drop(b);
}

// two allocators: a node cannot change owner, so the elem is copied and the address does
// not survive — the same rule splice_front follows
static void test_splice_node_across_allocators_copies_the_elem() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *a = make_list(3); // default: 0, 1, 2
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &b, 9));

    const trs_ListNode *before = node_at(a, 1);
    TRS_TEST_OK(trs_list_splice_node(b, nullptr, a, node_at(a, 1)));

    constexpr int32_t want_a[2] = {0, 2};
    constexpr int32_t want_b[2] = {9, 1};
    assert_elems(a, want_a, 2);
    assert_elems(b, want_b, 2);
    TEST_ASSERT_TRUE(before != trs_list_back_node(b));

    trs_list_drop(a);
    trs_list_drop(b);
    trs_al_arena_drop(arena);
}

/* ========== reverse ========== */

static void test_reverse_turns_the_list_around() {
    trs_List *l = make_list(5); // 0 .. 4

    trs_list_reverse(l);

    constexpr int32_t want[5] = {4, 3, 2, 1, 0};
    assert_elems(l, want, 5);

    trs_list_drop(l);
}

static void test_reverse_of_empty_and_single_changes_nothing() {
    trs_List *empty = make_list(0);
    trs_list_reverse(empty);
    assert_empty(empty);
    trs_list_drop(empty);

    trs_List *one = make_list(1);
    trs_list_reverse(one);
    constexpr int32_t want[1] = {0};
    assert_elems(one, want, 1);
    trs_list_drop(one);
}

// relinking, not rebuilding: the same nodes come back in the other order
static void test_reverse_keeps_every_node_address() {
    trs_List *l = make_list(4);

    const trs_ListNode *before[4];
    collect_nodes(l, before, 4);

    trs_list_reverse(l);

    assert_same_nodes(l, before, 4);
    TEST_ASSERT_EQUAL_PTR(before[3], trs_list_front_node(l));
    TEST_ASSERT_EQUAL_PTR(before[0], trs_list_back_node(l));

    trs_list_drop(l);
}

static void test_reverse_twice_is_the_original() {
    trs_List *l = make_list(6);

    trs_list_reverse(l);
    trs_list_reverse(l);

    constexpr int32_t want[6] = {0, 1, 2, 3, 4, 5};
    assert_elems(l, want, 6);

    trs_list_drop(l);
}

/* ========== sort ========== */

static void test_sort_orders_the_elems() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 5, 1, 9, 3, 7, 2));

    trs_list_sort(l, trs_cmp_i32);

    constexpr int32_t want[6] = {1, 2, 3, 5, 7, 9};
    assert_elems(l, want, 6);

    trs_list_drop(l);
}

static void test_sort_of_empty_and_single_is_a_noop() {
    trs_List *empty = make_list(0);
    trs_list_sort(empty, trs_cmp_i32);
    assert_empty(empty);
    trs_list_drop(empty);

    trs_List *one = make_list(1);
    trs_list_sort(one, trs_cmp_i32);
    constexpr int32_t want[1] = {0};
    assert_elems(one, want, 1);
    trs_list_drop(one);
}

static void test_sort_handles_duplicates() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 3, 1, 3, 1, 3));

    trs_list_sort(l, trs_cmp_i32);

    constexpr int32_t want[5] = {1, 1, 3, 3, 3};
    assert_elems(l, want, 5);

    trs_list_drop(l);
}

static void test_sort_takes_a_descending_comparator() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 5, 1, 9, 3));

    trs_list_sort(l, trs_cmp_desc_i32);

    constexpr int32_t want[4] = {9, 5, 3, 1};
    assert_elems(l, want, 4);

    trs_list_drop(l);
}

// every length up to 17 exercises a different split in the merge sort, and an odd one
// tells a half computed as len/2 from one computed as (len + 1)/2
static void test_sort_works_at_every_length() {
    for (size_t n = 0; n <= 17; ++n) {
        trs_List *l = nullptr;
        TRS_TEST_OK(TRS_LIST_NEW(int32_t, trs_al_default(), &l));

        // a descending run: the worst input for a sort that assumes anything
        for (size_t i = 0; i < n; ++i) {
            push_int(l, (int32_t) (n - i));
        }

        trs_list_sort(l, trs_cmp_i32);

        TEST_ASSERT_EQUAL_size_t(n, trs_list_len(l));
        int32_t prev = 0;
        size_t seen = 0;
        for (const trs_ListNode *node = trs_list_front_node(l); node; node = trs_list_node_next(node)) {
            const int32_t val = *TRS_LIST_NODE_ELEM_AS(int32_t, node);
            if (seen > 0) {
                TEST_ASSERT_TRUE_MESSAGE(prev <= val, "the sorted list is out of order");
            }
            prev = val;
            ++seen;
        }
        TEST_ASSERT_EQUAL_size_t(n, seen);

        trs_list_drop(l);
    }
}

// equal keys must keep the order they were pushed in — the second field is what shows it
static void test_sort_is_stable() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_NEW(Pair, trs_al_default(), &l));

    constexpr Pair src[6] = {{2, 1}, {1, 1}, {2, 2}, {1, 2}, {2, 3}, {1, 3}};
    for (size_t i = 0; i < 6; ++i) {
        TRS_TEST_OK(trs_list_push_back(l, &src[i]));
    }

    trs_list_sort(l, cmp_pair_a);

    constexpr Pair want[6] = {{1, 1}, {1, 2}, {1, 3}, {2, 1}, {2, 2}, {2, 3}};
    size_t i = 0;
    for (const trs_ListNode *node = trs_list_front_node(l); node; node = trs_list_node_next(node)) {
        const Pair *got = TRS_LIST_NODE_ELEM_AS(Pair, node);
        TEST_ASSERT_EQUAL_INT64(want[i].a, got->a);
        TEST_ASSERT_EQUAL_INT64(want[i].b, got->b);
        ++i;
    }
    TEST_ASSERT_EQUAL_size_t(6, i);

    trs_list_drop(l);
}

// the difference from sorting a copy: the nodes move, the elems stay in the node they
// were pushed into, so a pointer taken before the sort still names the same elem
static void test_sort_keeps_every_node_and_its_elem() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 5, 1, 9, 3));

    const trs_ListNode *before[4];
    collect_nodes(l, before, 4);
    const trs_ListNode *node_of_nine = before[2];

    trs_list_sort(l, trs_cmp_i32);

    assert_same_nodes(l, before, 4);
    TEST_ASSERT_EQUAL_INT32(9, *TRS_LIST_NODE_ELEM_AS(int32_t, node_of_nine));
    TEST_ASSERT_EQUAL_PTR(node_of_nine, trs_list_back_node(l));

    trs_list_drop(l);
}

// relinking asks the allocator for nothing at all
static void test_sort_never_allocates() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &l, 5, 1, 9, 3, 7, 2, 8));

    const size_t requests = trs_test_probe_requests(&probe);
    trs_list_sort(l, trs_cmp_i32);

    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    constexpr int32_t want[7] = {1, 2, 3, 5, 7, 8, 9};
    assert_elems(l, want, 7);

    trs_list_drop(l);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== merge ========== */

static void test_merge_interleaves_two_sorted_lists() {
    trs_List *a = nullptr;
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &a, 1, 4, 7));
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &b, 2, 3, 8, 9));

    TRS_TEST_OK(trs_list_merge(a, b, trs_cmp_i32));

    constexpr int32_t want[7] = {1, 2, 3, 4, 7, 8, 9};
    assert_elems(a, want, 7);
    assert_empty(b);

    trs_list_drop(a);
    trs_list_drop(b);
}

static void test_merge_with_an_empty_source_changes_nothing() {
    trs_List *a = make_list(3);
    trs_List *b = make_list(0);

    TRS_TEST_OK(trs_list_merge(a, b, trs_cmp_i32));

    constexpr int32_t want[3] = {0, 1, 2};
    assert_elems(a, want, 3);
    assert_empty(b);

    trs_list_drop(a);
    trs_list_drop(b);
}

static void test_merge_into_an_empty_list_takes_everything() {
    trs_List *a = make_list(0);
    trs_List *b = make_list(4);

    TRS_TEST_OK(trs_list_merge(a, b, trs_cmp_i32));

    constexpr int32_t want[4] = {0, 1, 2, 3};
    assert_elems(a, want, 4);
    assert_empty(b);

    trs_list_drop(a);
    trs_list_drop(b);
}

// equal elems of 'self' come before those of 'src', the same rule algo/merge follows
static void test_merge_keeps_equal_elems_of_self_first() {
    trs_List *a = nullptr;
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_NEW(Pair, trs_al_default(), &a));
    TRS_TEST_OK(TRS_LIST_NEW(Pair, trs_al_default(), &b));

    constexpr Pair from_a[2] = {{1, 100}, {2, 100}};
    constexpr Pair from_b[2] = {{1, 200}, {2, 200}};
    for (size_t i = 0; i < 2; ++i) {
        TRS_TEST_OK(trs_list_push_back(a, &from_a[i]));
        TRS_TEST_OK(trs_list_push_back(b, &from_b[i]));
    }

    TRS_TEST_OK(trs_list_merge(a, b, cmp_pair_a));

    constexpr Pair want[4] = {{1, 100}, {1, 200}, {2, 100}, {2, 200}};
    size_t i = 0;
    for (const trs_ListNode *node = trs_list_front_node(a); node; node = trs_list_node_next(node)) {
        const Pair *got = TRS_LIST_NODE_ELEM_AS(Pair, node);
        TEST_ASSERT_EQUAL_INT64(want[i].a, got->a);
        TEST_ASSERT_EQUAL_INT64(want[i].b, got->b);
        ++i;
    }
    TEST_ASSERT_EQUAL_size_t(4, i);

    trs_list_drop(a);
    trs_list_drop(b);
}

static void test_merge_on_one_allocator_never_allocates() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_List *a = nullptr;
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &a, 1, 4, 7));
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &b, 2, 3));

    const size_t requests = trs_test_probe_requests(&probe);
    TRS_TEST_OK(trs_list_merge(a, b, trs_cmp_i32));

    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    constexpr int32_t want[5] = {1, 2, 3, 4, 7};
    assert_elems(a, want, 5);

    trs_list_drop(a);
    trs_list_drop(b);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// two allocators: the elems are copied into nodes of 'self' and 'src' is emptied anyway
static void test_merge_across_allocators_copies_the_elems() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *a = nullptr;
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &a, 1, 5));
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &b, 2, 9));

    TRS_TEST_OK(trs_list_merge(a, b, trs_cmp_i32));

    constexpr int32_t want[4] = {1, 2, 5, 9};
    assert_elems(a, want, 4);
    assert_empty(b);
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_list_al(a));

    trs_list_drop(a);
    trs_list_drop(b);
    trs_al_arena_drop(arena);
}

/* ========== copy to span ========== */

static void test_copy_to_span_writes_front_to_back() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 5, 1, 9));

    int32_t got[3];
    trs_list_copy_to_span(l, TRS_SPAN_FROM_DATA_MUT(int32_t, got, 3));

    constexpr int32_t want[3] = {5, 1, 9};
    TEST_ASSERT_EQUAL_INT32_ARRAY(want, got, 3);
    assert_elems(l, want, 3);

    trs_list_drop(l);
}

static void test_copy_to_span_of_empty_writes_nothing() {
    trs_List *l = make_list(0);

    int32_t got[2] = {11, 22};
    trs_list_copy_to_span(l, TRS_SPAN_FROM_DATA_MUT(int32_t, got, 0));

    TEST_ASSERT_EQUAL_INT32(11, got[0]);
    TEST_ASSERT_EQUAL_INT32(22, got[1]);

    trs_list_drop(l);
}

static void test_copy_from_span_overwrites_every_elem() {
    trs_List *l = make_list(3); // 0, 1, 2

    constexpr int32_t src[3] = {7, 8, 9};
    trs_list_copy_from_span(l, TRS_SPAN_FROM_DATA(int32_t, src, 3));

    assert_elems(l, src, 3);

    trs_list_drop(l);
}

// the pair writes through the nodes rather than rebuilding them, so nothing is allocated
// and every borrowed node still points at its own list
static void test_copy_from_span_keeps_the_nodes() {
    trs_List *l = make_list(3);

    const trs_ListNode *before[3];
    collect_nodes(l, before, 3);

    constexpr int32_t src[3] = {7, 8, 9};
    trs_list_copy_from_span(l, TRS_SPAN_FROM_DATA(int32_t, src, 3));

    assert_same_nodes(l, before, 3);
    TEST_ASSERT_EQUAL_INT32(7, *TRS_LIST_NODE_ELEM_AS(int32_t, before[0]));

    trs_list_drop(l);
}

// the round trip the bridge exists for — and the contrast with trs_list_sort: here the
// elems move between nodes, so the node that held 9 now holds something else
static void test_the_copy_reaches_algo_and_comes_back() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 5, 1, 9, 3));

    const trs_ListNode *node_of_nine = node_at(l, 2);

    int32_t buf[4];
    const trs_SpanMut s = TRS_SPAN_FROM_DATA_MUT(int32_t, buf, 4);
    trs_list_copy_to_span(l, s);

    TEST_ASSERT_EQUAL_size_t(2, trs_span_max_elem(trs_span_mut_to_span(s), trs_cmp_i32));

    trs_span_sort(s, trs_cmp_i32);
    trs_list_copy_from_span(l, trs_span_mut_to_span(s));

    constexpr int32_t want[4] = {1, 3, 5, 9};
    assert_elems(l, want, 4);
    TEST_ASSERT_EQUAL_INT32(5, *TRS_LIST_NODE_ELEM_AS(int32_t, node_of_nine));

    trs_list_drop(l);
}

/* ========== allocation failure ========== */

static void test_new_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_List *l = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_LIST_NEW(int32_t, arena, &l));

    TEST_ASSERT_NULL(l); // out is untouched on failure

    trs_al_arena_drop(arena);
}

static void test_push_back_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &l, 1, 2));
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_LIST_PUSH_BACK(int32_t, l, 3));

    assert_elems(l, (int32_t[]){1, 2}, 2); // the list is exactly as it was

    trs_al_arena_drop(arena);
}

static void test_push_front_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &l, 1, 2));
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_LIST_PUSH_FRONT(int32_t, l, 0));

    assert_elems(l, (int32_t[]){1, 2}, 2);

    trs_al_arena_drop(arena);
}

static void test_insert_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &l, 1, 2));
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_LIST_INSERT_AFTER(int32_t, l, trs_list_front_node_mut(l), 9));

    assert_elems(l, (int32_t[]){1, 2}, 2);

    trs_al_arena_drop(arena);
}

// a from_data that dies halfway must not leak the nodes it already built;
// the arena cannot show that (its dealloc is a no-op), the probe can
static void test_from_data_rolls_back_the_partial_list() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_bare(&probe);

    constexpr int32_t src[] = {1, 2, 3, 4};
    trs_List *l = nullptr;
    trs_test_probe_fail_after_next(&probe, 3); // struct + 2 nodes, then refuse

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_LIST_FROM_DATA(int32_t, src, 4, &al, &l));

    TEST_ASSERT_NULL(l);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_copy_rolls_back_the_partial_clone() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_bare(&probe);

    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &l, 1, 2, 3));
    const size_t live = probe.live;

    trs_test_probe_fail_after_next(&probe, 2); // struct + 1 node, then refuse
    trs_List *c = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_list_copy(l, &c));

    TEST_ASSERT_NULL(c);
    TEST_ASSERT_EQUAL_size_t(live, probe.live); // nothing of the clone survived

    probe.fail_after = SIZE_MAX;
    trs_list_drop(l);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the target only allocates for the elems it has no node for, so the
// failing case is a target SHORTER than the source
static void test_copy_assign_leaves_the_target_untouched_on_failure() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_bare(&probe);

    trs_List *src = make_list(4);
    trs_List *dst = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, &al, &dst, 7, 8));
    const size_t live = probe.live;

    trs_test_probe_fail_after_next(&probe, 1); // one node of the two it needs
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_list_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){7, 8}, 2);
    TEST_ASSERT_EQUAL_size_t(live, probe.live);

    probe.fail_after = SIZE_MAX;
    trs_list_drop(dst);
    trs_list_drop(src);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_splice_across_allocators_reports_failure() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_bare(&probe);

    trs_List *dst = nullptr;
    TRS_TEST_OK(TRS_LIST_NEW(int32_t, &al, &dst));
    trs_List *src = make_list(3);

    trs_test_probe_fail_after_next(&probe, 1); // struct of the clone, then refuse
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_list_splice_back(dst, src));

    assert_elems(src, (int32_t[]){0, 1, 2}, 3); // the source keeps its nodes
    assert_empty(dst);

    probe.fail_after = SIZE_MAX;
    trs_list_drop(dst);
    trs_list_drop(src);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the copy path is the one that can fail: an arena with nothing left refuses the node
static void test_splice_node_across_allocators_reports_failure() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *a = make_list(3); // default: 0, 1, 2
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &b, 9));
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_list_splice_node(b, nullptr, a, node_at(a, 1)));

    constexpr int32_t want_a[3] = {0, 1, 2};
    constexpr int32_t want_b[1] = {9};
    assert_elems(a, want_a, 3);
    assert_elems(b, want_b, 1);

    trs_list_drop(a);
    trs_list_drop(b);
    trs_al_arena_drop(arena);
}

static void test_merge_across_allocators_reports_failure() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    trs_List *a = nullptr;
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, arena, &a, 1, 5));
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &b, 2, 9));
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_list_merge(a, b, trs_cmp_i32));

    constexpr int32_t want_a[2] = {1, 5};
    constexpr int32_t want_b[2] = {2, 9};
    assert_elems(a, want_a, 2);
    assert_elems(b, want_b, 2);

    trs_list_drop(a);
    trs_list_drop(b);
    trs_al_arena_drop(arena);
}

/* ========== macros ========== */

static void test_macro_of_builds_from_a_value_list() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 4, 5, 6));

    assert_elems(l, (int32_t[]){4, 5, 6}, 3);

    trs_list_drop(l);
}

static void test_macro_of_carries_wide_elems() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(Pair, trs_al_default(), &l, {1, 2}, {3, 4}));

    TEST_ASSERT_EQUAL_size_t(2, trs_list_len(l));
    TEST_ASSERT_EQUAL_INT64(2, TRS_LIST_FRONT_AS(Pair, l)->b);
    TEST_ASSERT_EQUAL_INT64(3, TRS_LIST_BACK_AS(Pair, l)->a);

    trs_list_drop(l);
}

static void test_macro_push_evaluates_its_value_once() {
    trs_List *l = make_list(0);
    int32_t next = 0;

    for (int i = 0; i < 3; ++i) {
        TRS_TEST_OK(TRS_LIST_PUSH_BACK(int32_t, l, next++));
    }

    TEST_ASSERT_EQUAL_INT32(3, next);
    assert_elems(l, (int32_t[]){0, 1, 2}, 3);

    trs_list_drop(l);
}

static void test_macro_node_elem_as_reads_and_writes() {
    trs_List *l = make_list(2);
    trs_ListNode *head = trs_list_front_node_mut(l);

    TEST_ASSERT_EQUAL_INT32(0, *TRS_LIST_NODE_ELEM_AS(int32_t, head));
    *TRS_LIST_NODE_ELEM_MUT_AS(int32_t, head) = 5;

    assert_elems(l, (int32_t[]){5, 1}, 2);

    trs_list_drop(l);
}

/* ========== compare ========== */

static void test_eq_matches_the_same_elems() {
    trs_List *a = make_list(4);
    trs_List *b = make_list(4);

    TEST_ASSERT_TRUE(trs_list_eq(a, a));
    TEST_ASSERT_TRUE(trs_list_eq(a, b));
    TEST_ASSERT_TRUE(trs_list_eq(b, a));
    TEST_ASSERT_TRUE(trs_list_eq_by(a, b, trs_eq_i32));

    trs_list_drop(a);
    trs_list_drop(b);
}

static void test_eq_parts_one_differing_elem() {
    trs_List *a = make_list(4);
    trs_List *b = make_list(4);
    TRS_TEST_OK(TRS_LIST_PUSH_BACK(int32_t, b, 99));
    trs_list_pop_front(b);

    TEST_ASSERT_FALSE(trs_list_eq(a, b));
    TEST_ASSERT_FALSE(trs_list_eq_by(a, b, trs_eq_i32));

    trs_list_drop(a);
    trs_list_drop(b);
}

static void test_eq_parts_different_lengths() {
    trs_List *a = make_list(4);
    trs_List *shorter = make_list(3);

    TEST_ASSERT_FALSE(trs_list_eq(a, shorter));
    TEST_ASSERT_FALSE(trs_list_eq(shorter, a));

    trs_list_drop(a);
    trs_list_drop(shorter);
}

static void test_eq_of_two_empties() {
    trs_List *a = make_list(0);
    trs_List *b = make_list(0);
    trs_List *one = make_list(1);

    TEST_ASSERT_TRUE(trs_list_eq(a, b));
    TEST_ASSERT_TRUE(trs_list_eq_by(a, b, trs_eq_i32));
    TEST_ASSERT_FALSE(trs_list_eq(a, one));

    trs_list_drop(a);
    trs_list_drop(b);
    trs_list_drop(one);
}

// the same elems in the other order are other contents, so the walk has to run front to
// back and not just count what it meets
static void test_eq_is_order_sensitive() {
    trs_List *a = make_list(4);
    trs_List *b = make_list(4);

    trs_list_reverse(b);
    TEST_ASSERT_FALSE(trs_list_eq(a, b));

    trs_list_reverse(b);
    TEST_ASSERT_TRUE(trs_list_eq(a, b));

    trs_list_drop(a);
    trs_list_drop(b);
}

static void test_eq_by_asks_the_equality() {
    constexpr Pair lhs[2] = {{1, 10}, {2, 20}};
    constexpr Pair rhs[2] = {{1, 70}, {2, 80}};

    trs_List *a = nullptr;
    trs_List *b = nullptr;
    TRS_TEST_OK(TRS_LIST_FROM_DATA(Pair, lhs, 2, trs_al_default(), &a));
    TRS_TEST_OK(TRS_LIST_FROM_DATA(Pair, rhs, 2, trs_al_default(), &b));

    TEST_ASSERT_FALSE(trs_list_eq(a, b));
    TEST_ASSERT_TRUE(trs_list_eq_by(a, b, trs_test_pair_eq_a));

    trs_list_drop(a);
    trs_list_drop(b);
}

/* ========== print ========== */

// a printer writes to a stream, so a case has to read one back. tmpfile is the portable
// way, the same one test/core/test_print.c takes
static void assert_prints(const char *expected, const trs_List *l) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);

    trs_list_fprint(l, stream, trs_fprint_i32);
    rewind(stream);

    char buf[128];
    const size_t n = fread(buf, 1, sizeof buf - 1, stream);
    buf[n] = '\0';
    fclose(stream);

    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_fprint_writes_the_elems() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 5, 3, 1));

    assert_prints("[5, 3, 1]\n", l);

    trs_list_drop(l);
}

static void test_fprint_of_a_single_elem_has_no_separator() {
    trs_List *l = nullptr;
    TRS_TEST_OK(TRS_LIST_OF(int32_t, trs_al_default(), &l, 7));

    assert_prints("[7]\n", l);

    trs_list_drop(l);
}

static void test_fprint_of_an_empty_list() {
    trs_List *l = make_list(0);

    assert_prints("[]\n", l);

    trs_list_drop(l);
}

// the stdout twin takes no stream, and C has no portable way to capture one and give it
// back — so a case can only say that it runs and reaches the same printer
static void test_print_writes_to_stdout() {
    trs_List *l = make_list(3);

    trs_list_print(l, trs_fprint_i32);

    trs_list_drop(l);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_empty);
    RUN_TEST(test_new_keeps_elem_size_and_allocator);
    RUN_TEST(test_from_data_copies_and_detaches_the_source);
    RUN_TEST(test_from_data_empty_is_empty);
    RUN_TEST(test_from_data_copies_whole_elems);
    RUN_TEST(test_from_span_copies_the_view);
    RUN_TEST(test_drop_null_is_noop);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_inherits_the_source_allocator);
    RUN_TEST(test_copy_with_builds_on_the_given_allocator);
    RUN_TEST(test_copy_with_reports_an_exhausted_target_arena);
    RUN_TEST(test_move_assign_hands_over_the_contents_on_one_allocator);
    RUN_TEST(test_move_assign_across_allocators_empties_the_source);
    RUN_TEST(test_move_assign_across_allocators_reports_an_exhausted_arena);
    RUN_TEST(test_move_assign_of_itself_changes_nothing);
    RUN_TEST(test_copy_of_empty_is_empty);
    RUN_TEST(test_copy_assign_grows_the_target);
    RUN_TEST(test_copy_assign_shrinks_the_target);
    RUN_TEST(test_copy_assign_from_empty_empties_the_target);
    RUN_TEST(test_copy_assign_to_empty_target);
    RUN_TEST(test_copy_assign_self_is_noop);
    RUN_TEST(test_copy_assign_keeps_the_target_allocator);
    RUN_TEST(test_copy_assign_reuses_the_target_nodes);

    RUN_TEST(test_len_counts_the_elems);
    RUN_TEST(test_elem_size_is_the_ctor_arg);
    RUN_TEST(test_al_is_the_ctor_arg);

    RUN_TEST(test_first_and_last_see_both_ends);
    RUN_TEST(test_first_and_last_meet_on_a_single_elem);
    RUN_TEST(test_first_mut_writes_through);
    RUN_TEST(test_last_mut_writes_through);

    RUN_TEST(test_nodes_of_an_empty_list_are_null);
    RUN_TEST(test_the_ends_are_the_same_node_on_a_single_elem);
    RUN_TEST(test_node_prev_mut_writes_through_the_backward_walk);
    RUN_TEST(test_next_and_prev_are_each_others_inverse);
    RUN_TEST(test_node_elem_mut_writes_through);

    RUN_TEST(test_push_back_appends);
    RUN_TEST(test_push_front_prepends);
    RUN_TEST(test_push_front_and_back_interleave);
    RUN_TEST(test_pop_front_drops_the_head);
    RUN_TEST(test_pop_back_drops_the_tail);
    RUN_TEST(test_popping_to_empty_resets_both_ends);
    RUN_TEST(test_push_after_emptying_works);

    RUN_TEST(test_insert_before_the_middle);
    RUN_TEST(test_insert_after_the_middle);
    RUN_TEST(test_insert_before_the_head_moves_the_head);
    RUN_TEST(test_insert_after_the_tail_moves_the_tail);
    RUN_TEST(test_the_new_node_is_reachable_from_the_anchor);
    RUN_TEST(test_insert_into_a_single_elem_list);
    RUN_TEST(test_remove_from_the_middle);
    RUN_TEST(test_remove_the_head);
    RUN_TEST(test_remove_the_tail);
    RUN_TEST(test_remove_the_only_node_empties_the_list);

    RUN_TEST(test_clear_empties_the_list);
    RUN_TEST(test_clear_of_an_empty_list_is_noop);
    RUN_TEST(test_clear_leaves_the_list_usable);
    RUN_TEST(test_clear_frees_every_node);

    RUN_TEST(test_splice_back_appends_and_empties_the_source);
    RUN_TEST(test_splice_front_prepends_and_empties_the_source);
    RUN_TEST(test_splice_into_an_empty_list);
    RUN_TEST(test_splice_front_into_an_empty_list);
    RUN_TEST(test_splice_from_an_empty_list_is_noop);
    RUN_TEST(test_splice_moves_the_nodes_without_allocating);
    RUN_TEST(test_splice_across_allocators_copies_the_elems);

    RUN_TEST(test_find_returns_the_first_node_holding_the_elem);
    RUN_TEST(test_find_of_a_missing_elem_is_null);
    RUN_TEST(test_find_of_an_empty_list_is_null);
    RUN_TEST(test_find_mut_gives_a_position_to_write_through);
    RUN_TEST(test_find_asks_the_equality_it_is_given);
    RUN_TEST(test_for_each_walks_front_to_back);
    RUN_TEST(test_for_each_over_an_empty_list_runs_no_body);
    RUN_TEST(test_for_each_mut_writes_through_every_position);

    RUN_TEST(test_swap_exchanges_the_contents);
    RUN_TEST(test_swap_keeps_the_nodes_alive);
    RUN_TEST(test_swap_self_is_noop);
    RUN_TEST(test_swap_with_an_empty_list_works_both_ways);

    RUN_TEST(test_splice_node_moves_one_node_between_lists);
    RUN_TEST(test_splice_node_keeps_the_node_address);
    RUN_TEST(test_splice_node_at_null_appends_to_the_back);
    RUN_TEST(test_splice_node_within_one_list_moves_the_elem);
    RUN_TEST(test_splice_node_to_the_front_of_the_same_list);
    RUN_TEST(test_splice_node_one_step_forward);
    RUN_TEST(test_splice_node_of_the_only_node_empties_the_source);
    RUN_TEST(test_splice_node_across_allocators_copies_the_elem);

    RUN_TEST(test_reverse_turns_the_list_around);
    RUN_TEST(test_reverse_of_empty_and_single_changes_nothing);
    RUN_TEST(test_reverse_keeps_every_node_address);
    RUN_TEST(test_reverse_twice_is_the_original);

    RUN_TEST(test_sort_orders_the_elems);
    RUN_TEST(test_sort_of_empty_and_single_is_a_noop);
    RUN_TEST(test_sort_handles_duplicates);
    RUN_TEST(test_sort_takes_a_descending_comparator);
    RUN_TEST(test_sort_works_at_every_length);
    RUN_TEST(test_sort_is_stable);
    RUN_TEST(test_sort_keeps_every_node_and_its_elem);
    RUN_TEST(test_sort_never_allocates);

    RUN_TEST(test_merge_interleaves_two_sorted_lists);
    RUN_TEST(test_merge_with_an_empty_source_changes_nothing);
    RUN_TEST(test_merge_into_an_empty_list_takes_everything);
    RUN_TEST(test_merge_keeps_equal_elems_of_self_first);
    RUN_TEST(test_merge_on_one_allocator_never_allocates);
    RUN_TEST(test_merge_across_allocators_copies_the_elems);

    RUN_TEST(test_copy_to_span_writes_front_to_back);
    RUN_TEST(test_copy_to_span_of_empty_writes_nothing);
    RUN_TEST(test_copy_from_span_overwrites_every_elem);
    RUN_TEST(test_copy_from_span_keeps_the_nodes);
    RUN_TEST(test_the_copy_reaches_algo_and_comes_back);

    RUN_TEST(test_new_reports_an_exhausted_arena);
    RUN_TEST(test_push_back_reports_an_exhausted_arena);
    RUN_TEST(test_push_front_reports_an_exhausted_arena);
    RUN_TEST(test_insert_reports_an_exhausted_arena);
    RUN_TEST(test_from_data_rolls_back_the_partial_list);
    RUN_TEST(test_copy_rolls_back_the_partial_clone);
    RUN_TEST(test_copy_assign_leaves_the_target_untouched_on_failure);
    RUN_TEST(test_splice_across_allocators_reports_failure);
    RUN_TEST(test_splice_node_across_allocators_reports_failure);
    RUN_TEST(test_merge_across_allocators_reports_failure);

    RUN_TEST(test_macro_of_builds_from_a_value_list);
    RUN_TEST(test_macro_of_carries_wide_elems);
    RUN_TEST(test_macro_push_evaluates_its_value_once);
    RUN_TEST(test_macro_node_elem_as_reads_and_writes);


    RUN_TEST(test_eq_matches_the_same_elems);
    RUN_TEST(test_eq_parts_one_differing_elem);
    RUN_TEST(test_eq_parts_different_lengths);
    RUN_TEST(test_eq_of_two_empties);
    RUN_TEST(test_eq_is_order_sensitive);
    RUN_TEST(test_eq_by_asks_the_equality);

    RUN_TEST(test_fprint_writes_the_elems);
    RUN_TEST(test_fprint_of_a_single_elem_has_no_separator);
    RUN_TEST(test_fprint_of_an_empty_list);
    RUN_TEST(test_print_writes_to_stdout);

    return UNITY_END();
}
