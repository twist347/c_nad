#include "nad/ds/list.h"
#include "nad/alloc/arena.h"
#include "nad/alloc/default.h"

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

static void push_int(nad_List *l, int32_t val) {
    NAD_TEST_OK(nad_list_push_back(l, &val));
}

// int32_t list holding 0, 1, ... len-1
static nad_List *make_list(size_t len) {
    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_NEW(int32_t, nad_al_default(), &l));

    for (size_t i = 0; i < len; ++i) {
        push_int(l, (int32_t) i);
    }
    return l;
}

// the whole contents, checked in BOTH directions: a broken next/prev pair
// leaves one of the two walks intact, so a one-way check misses half the bugs
static void assert_elems(const nad_List *l, const int32_t *want, size_t n) {
    TEST_ASSERT_EQUAL_size_t(n, nad_list_len(l));

    size_t seen = 0;
    for (const nad_ListNode *node = nad_list_first_node(l); node; node = nad_list_node_next(node)) {
        TEST_ASSERT_TRUE_MESSAGE(seen < n, "forward walk is longer than expected");
        TEST_ASSERT_EQUAL_INT32(want[seen], *NAD_LIST_NODE_ELEM_AS(int32_t, node));
        ++seen;
    }
    TEST_ASSERT_EQUAL_size_t(n, seen);

    seen = 0;
    for (const nad_ListNode *node = nad_list_last_node(l); node; node = nad_list_node_prev(node)) {
        TEST_ASSERT_TRUE_MESSAGE(seen < n, "backward walk is longer than expected");
        TEST_ASSERT_EQUAL_INT32(want[n - seen - 1], *NAD_LIST_NODE_ELEM_AS(int32_t, node));
        ++seen;
    }
    TEST_ASSERT_EQUAL_size_t(n, seen);

    if (n > 0) {
        TEST_ASSERT_EQUAL_INT32(want[0], *NAD_LIST_FIRST_AS(int32_t, l));
        TEST_ASSERT_EQUAL_INT32(want[n - 1], *NAD_LIST_LAST_AS(int32_t, l));
    } else {
        TEST_ASSERT_NULL(nad_list_first_node(l));
        TEST_ASSERT_NULL(nad_list_last_node(l));
    }
}

static void assert_empty(const nad_List *l) {
    assert_elems(l, nullptr, 0);
}

/* ========== lifetime ========== */

static void test_new_starts_empty() {
    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_NEW(int32_t, nad_al_default(), &l));

    assert_empty(l);

    nad_list_drop(l);
}

static void test_new_keeps_elem_size_and_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);
    TEST_ASSERT_NOT_NULL(arena);

    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_NEW(Pair, arena, &l));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), nad_list_elem_size(l));
    TEST_ASSERT_EQUAL_PTR(arena, nad_list_al(l));

    nad_list_drop(l);
    nad_al_arena_drop(arena);
}

static void test_from_data_copies_and_detaches_the_source() {
    int32_t src[] = {1, 2, 3};
    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_FROM_DATA(int32_t, src, 3, nad_al_default(), &l));

    src[0] = 99; // the list must not follow the source

    assert_elems(l, (int32_t[]){1, 2, 3}, 3);

    nad_list_drop(l);
}

static void test_from_data_empty_is_empty() {
    nad_List *l = nullptr;
    NAD_TEST_OK(nad_list_from_data(nullptr, 0, sizeof(int32_t), nad_al_default(), &l));

    assert_empty(l);

    nad_list_drop(l);
}

// a wide elem must travel whole, not truncated to a word
static void test_from_data_copies_whole_elems() {
    const Pair src[] = {{1, 2}, {3, 4}};
    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_FROM_DATA(Pair, src, 2, nad_al_default(), &l));

    const Pair *first = NAD_LIST_FIRST_AS(Pair, l);
    const Pair *last = NAD_LIST_LAST_AS(Pair, l);

    TEST_ASSERT_EQUAL_INT64(1, first->a);
    TEST_ASSERT_EQUAL_INT64(2, first->b);
    TEST_ASSERT_EQUAL_INT64(3, last->a);
    TEST_ASSERT_EQUAL_INT64(4, last->b);

    nad_list_drop(l);
}

static void test_from_span_copies_the_view() {
    const int32_t src[] = {5, 6, 7};
    nad_List *l = nullptr;
    NAD_TEST_OK(nad_list_from_span(NAD_SPAN_NEW(int32_t, src, 3), nad_al_default(), &l));

    assert_elems(l, (int32_t[]){5, 6, 7}, 3);

    nad_list_drop(l);
}

static void test_drop_null_is_noop() {
    nad_list_drop(nullptr);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    nad_List *l = make_list(3);
    nad_List *c = nullptr;
    NAD_TEST_OK(nad_list_copy(l, &c));

    nad_list_pop_back(c);
    push_int(l, 9);

    assert_elems(l, (int32_t[]){0, 1, 2, 9}, 4);
    assert_elems(c, (int32_t[]){0, 1}, 2);

    nad_list_drop(c);
    nad_list_drop(l);
}

static void test_copy_inherits_the_source_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, arena, &l, 1, 2));

    nad_List *c = nullptr;
    NAD_TEST_OK(nad_list_copy(l, &c));

    TEST_ASSERT_EQUAL_PTR(arena, nad_list_al(c));

    nad_al_arena_drop(arena);
}

static void test_copy_of_empty_is_empty() {
    nad_List *l = make_list(0);
    nad_List *c = nullptr;
    NAD_TEST_OK(nad_list_copy(l, &c));

    assert_empty(c);

    nad_list_drop(c);
    nad_list_drop(l);
}

static void test_copy_assign_grows_the_target() {
    nad_List *src = make_list(4);
    nad_List *dst = make_list(1);

    NAD_TEST_OK(nad_list_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){0, 1, 2, 3}, 4);
    assert_elems(src, (int32_t[]){0, 1, 2, 3}, 4);

    nad_list_drop(dst);
    nad_list_drop(src);
}

static void test_copy_assign_shrinks_the_target() {
    nad_List *src = make_list(1);
    nad_List *dst = make_list(4);

    NAD_TEST_OK(nad_list_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){0}, 1);

    nad_list_drop(dst);
    nad_list_drop(src);
}

static void test_copy_assign_from_empty_empties_the_target() {
    nad_List *src = make_list(0);
    nad_List *dst = make_list(3);

    NAD_TEST_OK(nad_list_copy_assign(src, dst));

    assert_empty(dst);
    push_int(dst, 7); // still usable
    assert_elems(dst, (int32_t[]){7}, 1);

    nad_list_drop(dst);
    nad_list_drop(src);
}

static void test_copy_assign_to_empty_target() {
    nad_List *src = make_list(2);
    nad_List *dst = make_list(0);

    NAD_TEST_OK(nad_list_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){0, 1}, 2);

    nad_list_drop(dst);
    nad_list_drop(src);
}

static void test_copy_assign_self_is_noop() {
    nad_List *l = make_list(3);

    NAD_TEST_OK(nad_list_copy_assign(l, l));

    assert_elems(l, (int32_t[]){0, 1, 2}, 3);

    nad_list_drop(l);
}

static void test_copy_assign_keeps_the_target_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    nad_List *src = make_list(2);
    nad_List *dst = nullptr;
    NAD_TEST_OK(NAD_LIST_NEW(int32_t, arena, &dst));

    NAD_TEST_OK(nad_list_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_PTR(arena, nad_list_al(dst));
    assert_elems(dst, (int32_t[]){0, 1}, 2);

    nad_list_drop(src);
    nad_al_arena_drop(arena);
}

// the shared prefix keeps its nodes, so assignment costs allocations
// only for the elems the target had no node for
static void test_copy_assign_reuses_the_target_nodes() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_bare(&probe);

    nad_List *src = make_list(2);
    nad_List *dst = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, &al, &dst, 7, 8));

    const nad_ListNode *head = nad_list_first_node(dst);
    const nad_ListNode *tail = nad_list_last_node(dst);
    const size_t before = nad_test_probe_requests(&probe);

    NAD_TEST_OK(nad_list_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(before, nad_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_PTR(head, nad_list_first_node(dst));
    TEST_ASSERT_EQUAL_PTR(tail, nad_list_last_node(dst));
    assert_elems(dst, (int32_t[]){0, 1}, 2);

    nad_list_drop(dst);
    nad_list_drop(src);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== info ========== */

static void test_len_counts_the_elems() {
    nad_List *l = make_list(0);
    TEST_ASSERT_EQUAL_size_t(0, nad_list_len(l));

    push_int(l, 1);
    TEST_ASSERT_EQUAL_size_t(1, nad_list_len(l));

    push_int(l, 2);
    TEST_ASSERT_EQUAL_size_t(2, nad_list_len(l));

    nad_list_pop_front(l);
    TEST_ASSERT_EQUAL_size_t(1, nad_list_len(l));

    nad_list_drop(l);
}

static void test_elem_size_is_the_ctor_arg() {
    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_NEW(Pair, nad_al_default(), &l));

    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), nad_list_elem_size(l));

    nad_list_drop(l);
}

static void test_al_is_the_ctor_arg() {
    nad_List *l = make_list(0);

    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_list_al(l));

    nad_list_drop(l);
}

/* ========== access ========== */

static void test_first_and_last_see_both_ends() {
    nad_List *l = make_list(3);

    TEST_ASSERT_EQUAL_INT32(0, *NAD_LIST_FIRST_AS(int32_t, l));
    TEST_ASSERT_EQUAL_INT32(2, *NAD_LIST_LAST_AS(int32_t, l));

    nad_list_drop(l);
}

static void test_first_and_last_meet_on_a_single_elem() {
    nad_List *l = make_list(1);

    TEST_ASSERT_EQUAL_PTR(nad_list_first(l), nad_list_last(l));

    nad_list_drop(l);
}

static void test_first_mut_writes_through() {
    nad_List *l = make_list(3);

    *NAD_LIST_FIRST_MUT_AS(int32_t, l) = 9;

    assert_elems(l, (int32_t[]){9, 1, 2}, 3);

    nad_list_drop(l);
}

static void test_last_mut_writes_through() {
    nad_List *l = make_list(3);

    *NAD_LIST_LAST_MUT_AS(int32_t, l) = 9;

    assert_elems(l, (int32_t[]){0, 1, 9}, 3);

    nad_list_drop(l);
}

/* ========== nodes ========== */

static void test_nodes_of_an_empty_list_are_null() {
    nad_List *l = make_list(0);

    TEST_ASSERT_NULL(nad_list_first_node(l));
    TEST_ASSERT_NULL(nad_list_last_node(l));

    nad_list_drop(l);
}

static void test_the_ends_are_the_same_node_on_a_single_elem() {
    nad_List *l = make_list(1);

    TEST_ASSERT_EQUAL_PTR(nad_list_first_node(l), nad_list_last_node(l));
    TEST_ASSERT_NULL(nad_list_node_next(nad_list_first_node(l)));
    TEST_ASSERT_NULL(nad_list_node_prev(nad_list_first_node(l)));

    nad_list_drop(l);
}

static void test_next_and_prev_are_each_others_inverse() {
    nad_List *l = make_list(3);

    const nad_ListNode *head = nad_list_first_node(l);
    const nad_ListNode *mid = nad_list_node_next(head);
    const nad_ListNode *tail = nad_list_node_next(mid);

    TEST_ASSERT_EQUAL_PTR(tail, nad_list_last_node(l));
    TEST_ASSERT_NULL(nad_list_node_next(tail));
    TEST_ASSERT_EQUAL_PTR(mid, nad_list_node_prev(tail));
    TEST_ASSERT_EQUAL_PTR(head, nad_list_node_prev(mid));
    TEST_ASSERT_NULL(nad_list_node_prev(head));

    nad_list_drop(l);
}

static void test_node_elem_mut_writes_through() {
    nad_List *l = make_list(3);

    nad_ListNode *mid = nad_list_node_next_mut(nad_list_first_node_mut(l));
    *NAD_LIST_NODE_ELEM_MUT_AS(int32_t, mid) = 9;

    assert_elems(l, (int32_t[]){0, 9, 2}, 3);

    nad_list_drop(l);
}

/* ========== push / pop ========== */

static void test_push_back_appends() {
    nad_List *l = make_list(0);

    push_int(l, 1);
    push_int(l, 2);

    assert_elems(l, (int32_t[]){1, 2}, 2);

    nad_list_drop(l);
}

static void test_push_front_prepends() {
    nad_List *l = make_list(0);
    int32_t one = 1;
    int32_t two = 2;

    NAD_TEST_OK(nad_list_push_front(l, &one));
    NAD_TEST_OK(nad_list_push_front(l, &two));

    assert_elems(l, (int32_t[]){2, 1}, 2);

    nad_list_drop(l);
}

// both ends grow at once: the two chains must stay consistent
static void test_push_front_and_back_interleave() {
    nad_List *l = make_list(0);

    NAD_TEST_OK(NAD_LIST_PUSH_BACK(int32_t, l, 1));
    NAD_TEST_OK(NAD_LIST_PUSH_FRONT(int32_t, l, 0));
    NAD_TEST_OK(NAD_LIST_PUSH_BACK(int32_t, l, 2));
    NAD_TEST_OK(NAD_LIST_PUSH_FRONT(int32_t, l, -1));

    assert_elems(l, (int32_t[]){-1, 0, 1, 2}, 4);

    nad_list_drop(l);
}

static void test_pop_front_drops_the_head() {
    nad_List *l = make_list(3);

    nad_list_pop_front(l);

    assert_elems(l, (int32_t[]){1, 2}, 2);

    nad_list_drop(l);
}

static void test_pop_back_drops_the_tail() {
    nad_List *l = make_list(3);

    nad_list_pop_back(l);

    assert_elems(l, (int32_t[]){0, 1}, 2);

    nad_list_drop(l);
}

static void test_popping_to_empty_resets_both_ends() {
    nad_List *l = make_list(2);

    nad_list_pop_front(l);
    nad_list_pop_back(l);

    assert_empty(l);

    nad_list_drop(l);
}

static void test_push_after_emptying_works() {
    nad_List *l = make_list(2);

    nad_list_pop_back(l);
    nad_list_pop_back(l);
    push_int(l, 7);

    assert_elems(l, (int32_t[]){7}, 1);

    nad_list_drop(l);
}

/* ========== insert / remove ========== */

static void test_insert_before_the_middle() {
    nad_List *l = make_list(3);
    nad_ListNode *mid = nad_list_node_next_mut(nad_list_first_node_mut(l));

    NAD_TEST_OK(NAD_LIST_INSERT_BEFORE(int32_t, l, mid, 9));

    assert_elems(l, (int32_t[]){0, 9, 1, 2}, 4);

    nad_list_drop(l);
}

static void test_insert_after_the_middle() {
    nad_List *l = make_list(3);
    nad_ListNode *mid = nad_list_node_next_mut(nad_list_first_node_mut(l));

    NAD_TEST_OK(NAD_LIST_INSERT_AFTER(int32_t, l, mid, 9));

    assert_elems(l, (int32_t[]){0, 1, 9, 2}, 4);

    nad_list_drop(l);
}

static void test_insert_before_the_head_moves_the_head() {
    nad_List *l = make_list(2);

    NAD_TEST_OK(NAD_LIST_INSERT_BEFORE(int32_t, l, nad_list_first_node_mut(l), 9));

    assert_elems(l, (int32_t[]){9, 0, 1}, 3);

    nad_list_drop(l);
}

static void test_insert_after_the_tail_moves_the_tail() {
    nad_List *l = make_list(2);

    NAD_TEST_OK(NAD_LIST_INSERT_AFTER(int32_t, l, nad_list_last_node_mut(l), 9));

    assert_elems(l, (int32_t[]){0, 1, 9}, 3);

    nad_list_drop(l);
}

// the header promises where the new node lands
static void test_the_new_node_is_reachable_from_the_anchor() {
    nad_List *l = make_list(3);
    nad_ListNode *mid = nad_list_node_next_mut(nad_list_first_node_mut(l));

    NAD_TEST_OK(NAD_LIST_INSERT_BEFORE(int32_t, l, mid, 7));
    NAD_TEST_OK(NAD_LIST_INSERT_AFTER(int32_t, l, mid, 8));

    TEST_ASSERT_EQUAL_INT32(7, *NAD_LIST_NODE_ELEM_AS(int32_t, nad_list_node_prev(mid)));
    TEST_ASSERT_EQUAL_INT32(8, *NAD_LIST_NODE_ELEM_AS(int32_t, nad_list_node_next(mid)));

    nad_list_drop(l);
}

static void test_insert_into_a_single_elem_list() {
    nad_List *l = make_list(1);
    nad_ListNode *only = nad_list_first_node_mut(l);

    NAD_TEST_OK(NAD_LIST_INSERT_BEFORE(int32_t, l, only, -1));
    NAD_TEST_OK(NAD_LIST_INSERT_AFTER(int32_t, l, only, 1));

    assert_elems(l, (int32_t[]){-1, 0, 1}, 3);

    nad_list_drop(l);
}

static void test_remove_from_the_middle() {
    nad_List *l = make_list(3);
    nad_ListNode *mid = nad_list_node_next_mut(nad_list_first_node_mut(l));

    nad_list_remove(l, mid);

    assert_elems(l, (int32_t[]){0, 2}, 2);

    nad_list_drop(l);
}

static void test_remove_the_head() {
    nad_List *l = make_list(3);

    nad_list_remove(l, nad_list_first_node_mut(l));

    assert_elems(l, (int32_t[]){1, 2}, 2);

    nad_list_drop(l);
}

static void test_remove_the_tail() {
    nad_List *l = make_list(3);

    nad_list_remove(l, nad_list_last_node_mut(l));

    assert_elems(l, (int32_t[]){0, 1}, 2);

    nad_list_drop(l);
}

static void test_remove_the_only_node_empties_the_list() {
    nad_List *l = make_list(1);

    nad_list_remove(l, nad_list_first_node_mut(l));

    assert_empty(l);

    nad_list_drop(l);
}

/* ========== clear ========== */

static void test_clear_empties_the_list() {
    nad_List *l = make_list(3);

    nad_list_clear(l);

    assert_empty(l);

    nad_list_drop(l);
}

static void test_clear_of_an_empty_list_is_noop() {
    nad_List *l = make_list(0);

    nad_list_clear(l);
    nad_list_clear(l);

    assert_empty(l);

    nad_list_drop(l);
}

static void test_clear_leaves_the_list_usable() {
    nad_List *l = make_list(3);

    nad_list_clear(l);
    push_int(l, 7);
    NAD_TEST_OK(NAD_LIST_PUSH_FRONT(int32_t, l, 6));

    assert_elems(l, (int32_t[]){6, 7}, 2);

    nad_list_drop(l);
}

static void test_clear_frees_every_node() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_bare(&probe);

    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, &al, &l, 1, 2, 3));

    nad_list_clear(l);

    TEST_ASSERT_EQUAL_size_t(1, probe.live); // the list struct itself

    nad_list_drop(l);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== splice ========== */

static void test_splice_back_appends_and_empties_the_source() {
    nad_List *dst = make_list(2);
    nad_List *src = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, nad_al_default(), &src, 7, 8));

    NAD_TEST_OK(nad_list_splice_back(dst, src));

    assert_elems(dst, (int32_t[]){0, 1, 7, 8}, 4);
    assert_empty(src);

    nad_list_drop(src);
    nad_list_drop(dst);
}

static void test_splice_front_prepends_and_empties_the_source() {
    nad_List *dst = make_list(2);
    nad_List *src = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, nad_al_default(), &src, 7, 8));

    NAD_TEST_OK(nad_list_splice_front(dst, src));

    assert_elems(dst, (int32_t[]){7, 8, 0, 1}, 4);
    assert_empty(src);

    nad_list_drop(src);
    nad_list_drop(dst);
}

static void test_splice_into_an_empty_list() {
    nad_List *dst = make_list(0);
    nad_List *src = make_list(3);

    NAD_TEST_OK(nad_list_splice_back(dst, src));

    assert_elems(dst, (int32_t[]){0, 1, 2}, 3);
    assert_empty(src);

    nad_list_drop(src);
    nad_list_drop(dst);
}

static void test_splice_front_into_an_empty_list() {
    nad_List *dst = make_list(0);
    nad_List *src = make_list(2);

    NAD_TEST_OK(nad_list_splice_front(dst, src));

    assert_elems(dst, (int32_t[]){0, 1}, 2);
    assert_empty(src);

    nad_list_drop(src);
    nad_list_drop(dst);
}

static void test_splice_from_an_empty_list_is_noop() {
    nad_List *dst = make_list(2);
    nad_List *src = make_list(0);

    NAD_TEST_OK(nad_list_splice_back(dst, src));
    NAD_TEST_OK(nad_list_splice_front(dst, src));

    assert_elems(dst, (int32_t[]){0, 1}, 2);
    assert_empty(src);

    nad_list_drop(src);
    nad_list_drop(dst);
}

// sharing an allocator makes splice a pointer rewire: no allocation,
// and the very same nodes end up in the target
static void test_splice_moves_the_nodes_without_allocating() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_bare(&probe);

    nad_List *dst = nullptr;
    nad_List *src = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, &al, &dst, 1, 2));
    NAD_TEST_OK(NAD_LIST_OF(int32_t, &al, &src, 3, 4));

    const nad_ListNode *src_head = nad_list_first_node(src);
    const size_t before = nad_test_probe_requests(&probe);

    NAD_TEST_OK(nad_list_splice_back(dst, src));

    TEST_ASSERT_EQUAL_size_t(before, nad_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_PTR(src_head, nad_list_node_next(nad_list_node_next(nad_list_first_node(dst))));
    assert_elems(dst, (int32_t[]){1, 2, 3, 4}, 4);

    nad_list_drop(src);
    nad_list_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// across allocators the nodes cannot travel, so the elems are copied
// into the target's allocator and the source is emptied all the same
static void test_splice_across_allocators_copies_the_elems() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_bare(&probe);

    nad_List *dst = make_list(2);
    nad_List *src = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, &al, &src, 7, 8));

    NAD_TEST_OK(nad_list_splice_back(dst, src));

    assert_elems(dst, (int32_t[]){0, 1, 7, 8}, 4);
    assert_empty(src);
    TEST_ASSERT_EQUAL_size_t(1, probe.live); // src kept nothing but its own struct

    nad_list_drop(src);
    nad_list_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== swap ========== */

static void test_swap_exchanges_the_contents() {
    nad_List *a = make_list(3);
    nad_List *b = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, nad_al_default(), &b, 9));

    nad_list_swap(a, b);

    assert_elems(a, (int32_t[]){9}, 1);
    assert_elems(b, (int32_t[]){0, 1, 2}, 3);

    nad_list_drop(b);
    nad_list_drop(a);
}

// the nodes do not move, they only change list — that is what makes
// swap O(1) and every borrowed node still valid afterwards
static void test_swap_keeps_the_nodes_alive() {
    nad_List *a = make_list(2);
    nad_List *b = make_list(1);

    const nad_ListNode *a_head = nad_list_first_node(a);
    const nad_ListNode *b_head = nad_list_first_node(b);

    nad_list_swap(a, b);

    TEST_ASSERT_EQUAL_PTR(a_head, nad_list_first_node(b));
    TEST_ASSERT_EQUAL_PTR(b_head, nad_list_first_node(a));

    nad_list_drop(b);
    nad_list_drop(a);
}

static void test_swap_self_is_noop() {
    nad_List *l = make_list(3);

    nad_list_swap(l, l);

    assert_elems(l, (int32_t[]){0, 1, 2}, 3);

    nad_list_drop(l);
}

static void test_swap_with_an_empty_list_works_both_ways() {
    nad_List *l = make_list(2);
    nad_List *none = make_list(0);

    nad_list_swap(l, none);
    assert_empty(l);
    assert_elems(none, (int32_t[]){0, 1}, 2);

    nad_list_swap(l, none);
    assert_elems(l, (int32_t[]){0, 1}, 2);
    assert_empty(none);

    push_int(none, 7); // both still usable
    assert_elems(none, (int32_t[]){7}, 1);

    nad_list_drop(none);
    nad_list_drop(l);
}

/* ========== allocation failure ========== */

static void test_new_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    nad_test_arena_leave(arena, 0);

    nad_List *l = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, NAD_LIST_NEW(int32_t, arena, &l));

    TEST_ASSERT_NULL(l); // out is untouched on failure

    nad_al_arena_drop(arena);
}

static void test_push_back_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, arena, &l, 1, 2));
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, NAD_LIST_PUSH_BACK(int32_t, l, 3));

    assert_elems(l, (int32_t[]){1, 2}, 2); // the list is exactly as it was

    nad_al_arena_drop(arena);
}

static void test_push_front_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, arena, &l, 1, 2));
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, NAD_LIST_PUSH_FRONT(int32_t, l, 0));

    assert_elems(l, (int32_t[]){1, 2}, 2);

    nad_al_arena_drop(arena);
}

static void test_insert_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, arena, &l, 1, 2));
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_LIST_INSERT_AFTER(int32_t, l, nad_list_first_node_mut(l), 9)
    );

    assert_elems(l, (int32_t[]){1, 2}, 2);

    nad_al_arena_drop(arena);
}

// a from_data that dies halfway must not leak the nodes it already built;
// the arena cannot show that (its dealloc is a no-op), the probe can
static void test_from_data_rolls_back_the_partial_list() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_bare(&probe);

    const int32_t src[] = {1, 2, 3, 4};
    nad_List *l = nullptr;
    nad_test_probe_fail_after_next(&probe, 3); // struct + 2 nodes, then refuse

    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_LIST_FROM_DATA(int32_t, src, 4, &al, &l)
    );

    TEST_ASSERT_NULL(l);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_copy_rolls_back_the_partial_clone() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_bare(&probe);

    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, &al, &l, 1, 2, 3));
    const size_t live = probe.live;

    nad_test_probe_fail_after_next(&probe, 2); // struct + 1 node, then refuse
    nad_List *c = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_list_copy(l, &c));

    TEST_ASSERT_NULL(c);
    TEST_ASSERT_EQUAL_size_t(live, probe.live); // nothing of the clone survived

    probe.fail_after = SIZE_MAX;
    nad_list_drop(l);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the target only allocates for the elems it has no node for, so the
// failing case is a target SHORTER than the source
static void test_copy_assign_leaves_the_target_untouched_on_failure() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_bare(&probe);

    nad_List *src = make_list(4);
    nad_List *dst = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, &al, &dst, 7, 8));
    const size_t live = probe.live;

    nad_test_probe_fail_after_next(&probe, 1); // one node of the two it needs
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_list_copy_assign(src, dst));

    assert_elems(dst, (int32_t[]){7, 8}, 2);
    TEST_ASSERT_EQUAL_size_t(live, probe.live);

    probe.fail_after = SIZE_MAX;
    nad_list_drop(dst);
    nad_list_drop(src);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_splice_across_allocators_reports_failure() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_bare(&probe);

    nad_List *dst = nullptr;
    NAD_TEST_OK(NAD_LIST_NEW(int32_t, &al, &dst));
    nad_List *src = make_list(3);

    nad_test_probe_fail_after_next(&probe, 1); // struct of the clone, then refuse
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_list_splice_back(dst, src));

    assert_elems(src, (int32_t[]){0, 1, 2}, 3); // the source keeps its nodes
    assert_empty(dst);

    probe.fail_after = SIZE_MAX;
    nad_list_drop(dst);
    nad_list_drop(src);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== macros ========== */

static void test_macro_of_builds_from_a_value_list() {
    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(int32_t, nad_al_default(), &l, 4, 5, 6));

    assert_elems(l, (int32_t[]){4, 5, 6}, 3);

    nad_list_drop(l);
}

static void test_macro_of_carries_wide_elems() {
    nad_List *l = nullptr;
    NAD_TEST_OK(NAD_LIST_OF(Pair, nad_al_default(), &l, {1, 2}, {3, 4}));

    TEST_ASSERT_EQUAL_size_t(2, nad_list_len(l));
    TEST_ASSERT_EQUAL_INT64(2, NAD_LIST_FIRST_AS(Pair, l)->b);
    TEST_ASSERT_EQUAL_INT64(3, NAD_LIST_LAST_AS(Pair, l)->a);

    nad_list_drop(l);
}

static void test_macro_push_evaluates_its_value_once() {
    nad_List *l = make_list(0);
    int32_t next = 0;

    for (int i = 0; i < 3; ++i) {
        NAD_TEST_OK(NAD_LIST_PUSH_BACK(int32_t, l, next++));
    }

    TEST_ASSERT_EQUAL_INT32(3, next);
    assert_elems(l, (int32_t[]){0, 1, 2}, 3);

    nad_list_drop(l);
}

static void test_macro_node_elem_as_reads_and_writes() {
    nad_List *l = make_list(2);
    nad_ListNode *head = nad_list_first_node_mut(l);

    TEST_ASSERT_EQUAL_INT32(0, *NAD_LIST_NODE_ELEM_AS(int32_t, head));
    *NAD_LIST_NODE_ELEM_MUT_AS(int32_t, head) = 5;

    assert_elems(l, (int32_t[]){5, 1}, 2);

    nad_list_drop(l);
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

    RUN_TEST(test_swap_exchanges_the_contents);
    RUN_TEST(test_swap_keeps_the_nodes_alive);
    RUN_TEST(test_swap_self_is_noop);
    RUN_TEST(test_swap_with_an_empty_list_works_both_ways);

    RUN_TEST(test_new_reports_an_exhausted_arena);
    RUN_TEST(test_push_back_reports_an_exhausted_arena);
    RUN_TEST(test_push_front_reports_an_exhausted_arena);
    RUN_TEST(test_insert_reports_an_exhausted_arena);
    RUN_TEST(test_from_data_rolls_back_the_partial_list);
    RUN_TEST(test_copy_rolls_back_the_partial_clone);
    RUN_TEST(test_copy_assign_leaves_the_target_untouched_on_failure);
    RUN_TEST(test_splice_across_allocators_reports_failure);

    RUN_TEST(test_macro_of_builds_from_a_value_list);
    RUN_TEST(test_macro_of_carries_wide_elems);
    RUN_TEST(test_macro_push_evaluates_its_value_once);
    RUN_TEST(test_macro_node_elem_as_reads_and_writes);

    return UNITY_END();
}
