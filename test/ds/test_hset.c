#include "terse/ds/hset.h"
#include "terse/alloc/arena.h"
#include "terse/alloc/default.h"
#include "terse/core/cmp.h"
#include "terse/core/hash.h"
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

/* ========== helpers ========== */

// every key in one bucket, so the chain is walked instead of hit at its head
static trs_Hash hash_all_alike(const void *x) {
    TRS_UNUSED(x);

    return 0;
}

static trs_Hash hash_pair(const void *x) {
    const Pair *p = x;

    return trs_hash_combine(trs_hash_i64(&p->a), trs_hash_i64(&p->b));
}

static bool eq_pair(const void *lhs, const void *rhs) {
    const Pair *a = lhs;
    const Pair *b = rhs;

    return a->a == b->a && a->b == b->b;
}

[[nodiscard]]
static size_t align_up_to(size_t val, size_t alignment) {
    return (val + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]]
static trs_HSet *make_set(trs_Hasher hasher) {
    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, hasher, trs_eq_i32, trs_al_default(), &s));

    return s;
}

static void put(trs_HSet *s, int32_t key) {
    TRS_TEST_OK(TRS_HSET_INSERT(int32_t, s, key, nullptr));
}

[[nodiscard]]
static trs_HSet *make_filled(trs_Hasher hasher, int32_t n) {
    trs_HSet *s = make_set(hasher);
    for (int32_t i = 0; i < n; ++i) {
        put(s, i);
    }
    return s;
}

static void assert_has(const trs_HSet *s, int32_t key) {
    TEST_ASSERT_TRUE(TRS_HSET_CONTAINS(int32_t, s, key));
    TEST_ASSERT_NOT_NULL(TRS_HSET_FIND(int32_t, s, key));
}

static void assert_missing(const trs_HSet *s, int32_t key) {
    TEST_ASSERT_FALSE(TRS_HSET_CONTAINS(int32_t, s, key));
    TEST_ASSERT_NULL(TRS_HSET_FIND(int32_t, s, key));
}

// the walk reaches exactly 'len' keys and every one of them is in the set
static void assert_walk_sees_everything(const trs_HSet *s) {
    size_t seen = 0;
    TRS_HSET_FOR_EACH(node, s) {
        assert_has(s, *TRS_HSET_NODE_KEY_AS(int32_t, node));
        ++seen;
    }
    TEST_ASSERT_EQUAL_size_t(trs_hset_len(s), seen);
}

/* ========== the node layout ========== */

// the whole reason ds/hset goes through internal/hmap_impl.h rather than storing a dummy
// byte: with no value behind the key there is nothing to pad the key to, so a set's node
// is a map's node minus the value AND minus the padding that would have aligned it
static void test_a_set_node_is_a_map_node_without_the_value() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, &al, &s));
    TRS_TEST_OK(TRS_HSET_INSERT(int32_t, s, 1, nullptr));
    const size_t set_node = probe.last_alloc_size; // the node is the last block a first insert takes

    trs_HMap *m = nullptr;
    TRS_TEST_OK(TRS_HMAP_NEW(int32_t, int32_t, trs_hash_i32, trs_eq_i32, &al, &m));
    TRS_TEST_OK(TRS_HMAP_INSERT(int32_t, int32_t, m, 1, 10, nullptr));
    const size_t map_node = probe.last_alloc_size;

    const size_t padding = align_up_to(sizeof(int32_t), alignof(max_align_t)) - sizeof(int32_t);
    TEST_ASSERT_TRUE(set_node < map_node);
    TEST_ASSERT_EQUAL_size_t(set_node + padding + sizeof(int32_t), map_node);

    trs_hset_drop(s);
    trs_hmap_drop(m);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== lifetime ========== */

static void test_new_starts_empty() {
    trs_HSet *s = make_set(trs_hash_i32);

    TEST_ASSERT_EQUAL_size_t(0, trs_hset_len(s));
    TEST_ASSERT_EQUAL_size_t(0, trs_hset_bucket_count(s));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), trs_hset_key_size(s));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_hset_al(s));
    TEST_ASSERT_EQUAL_PTR(trs_hash_i32, trs_hset_hasher(s));
    TEST_ASSERT_EQUAL_PTR(trs_eq_i32, trs_hset_key_eq(s));
    TEST_ASSERT_NULL(trs_hset_first_node(s));

    trs_hset_drop(s);
}

static void test_new_cap_reserves_buckets_without_keys() {
    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW_CAP(int32_t, 100, trs_hash_i32, trs_eq_i32, trs_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(0, trs_hset_len(s));
    TEST_ASSERT_TRUE(trs_hset_bucket_count(s) >= 100);

    trs_hset_drop(s);
}

static void test_drop_null_is_noop() {
    trs_hset_drop(nullptr);
}

// three blocks go into a filled set — the set header, the map behind it, and the buckets —
// plus one per key, and drop must hand back all of them
static void test_drop_hands_back_everything() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, &al, &s));
    for (int32_t i = 0; i < 20; ++i) {
        put(s, i);
    }

    trs_hset_drop(s);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the set header is taken after the map, and a refusal of it must not strand the map
static void test_a_refused_header_frees_the_map() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_test_probe_fail_after_next(&probe, 1);

    trs_HSet *s = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, &al, &s));

    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== insert and lookup ========== */

static void test_insert_then_contains() {
    trs_HSet *s = make_set(trs_hash_i32);

    put(s, 1);
    put(s, 2);

    assert_has(s, 1);
    assert_has(s, 2);
    assert_missing(s, 3);
    TEST_ASSERT_EQUAL_size_t(2, trs_hset_len(s));

    trs_hset_drop(s);
}

static void test_contains_on_an_empty_set_is_false() {
    trs_HSet *s = make_set(trs_hash_i32);

    assert_missing(s, 1);

    trs_hset_drop(s);
}

// a set holds a key once however many times it is put in
static void test_inserting_a_key_twice_keeps_one_entry() {
    trs_HSet *s = make_set(trs_hash_i32);

    put(s, 1);
    put(s, 1);
    put(s, 1);

    TEST_ASSERT_EQUAL_size_t(1, trs_hset_len(s));
    assert_has(s, 1);

    trs_hset_drop(s);
}

static void test_insert_reports_whether_the_key_was_new() {
    trs_HSet *s = make_set(trs_hash_i32);

    bool is_new = false;
    TRS_TEST_OK(TRS_HSET_INSERT(int32_t, s, 1, &is_new));
    TEST_ASSERT_TRUE(is_new);

    TRS_TEST_OK(TRS_HSET_INSERT(int32_t, s, 1, &is_new));
    TEST_ASSERT_FALSE(is_new);

    TRS_TEST_OK(TRS_HSET_INSERT(int32_t, s, 2, &is_new));
    TEST_ASSERT_TRUE(is_new);

    trs_hset_drop(s);
}

static void test_find_gives_the_key_back() {
    trs_HSet *s = make_filled(trs_hash_i32, 5);

    const trs_HSetNode *node = TRS_HSET_FIND(int32_t, s, 3);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT32(3, *TRS_HSET_NODE_KEY_AS(int32_t, node));

    trs_hset_drop(s);
}

static void test_a_set_whose_keys_all_collide_still_finds_them() {
    trs_HSet *s = make_filled(hash_all_alike, 20);

    TEST_ASSERT_EQUAL_size_t(20, trs_hset_len(s));
    for (int32_t i = 0; i < 20; ++i) {
        assert_has(s, i);
    }
    assert_missing(s, 20);

    trs_hset_drop(s);
}

static void test_wide_keys_travel_whole() {
    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(Pair, hash_pair, eq_pair, trs_al_default(), &s));

    constexpr Pair a = {1, 2};
    constexpr Pair b = {1, 3};
    TRS_TEST_OK(trs_hset_insert(s, &a, nullptr));

    TEST_ASSERT_TRUE(trs_hset_contains(s, &a));
    TEST_ASSERT_FALSE(trs_hset_contains(s, &b)); // the second field is part of the key
    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), trs_hset_key_size(s));

    trs_hset_drop(s);
}

/* ========== growth ========== */

static void test_every_key_survives_the_growths() {
    trs_HSet *s = make_filled(trs_hash_i32, 300);

    TEST_ASSERT_EQUAL_size_t(300, trs_hset_len(s));
    for (int32_t i = 0; i < 300; ++i) {
        assert_has(s, i);
    }
    assert_missing(s, 300);

    trs_hset_drop(s);
}

static void test_a_borrowed_node_survives_every_growth() {
    trs_HSet *s = make_filled(trs_hash_i32, 4);

    const trs_HSetNode *held[4];
    for (int32_t i = 0; i < 4; ++i) {
        held[i] = TRS_HSET_FIND(int32_t, s, i);
        TEST_ASSERT_NOT_NULL(held[i]);
    }

    for (int32_t i = 4; i < 500; ++i) {
        put(s, i);
    }

    for (int32_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_PTR(held[i], TRS_HSET_FIND(int32_t, s, i));
        TEST_ASSERT_EQUAL_INT32(i, *TRS_HSET_NODE_KEY_AS(int32_t, held[i]));
    }

    trs_hset_drop(s);
}

static void test_reserve_grows_the_buckets_only() {
    trs_HSet *s = make_filled(trs_hash_i32, 4);

    TRS_TEST_OK(trs_hset_reserve(s, 1000));

    TEST_ASSERT_TRUE(trs_hset_bucket_count(s) >= 1000);
    TEST_ASSERT_EQUAL_size_t(4, trs_hset_len(s));
    assert_has(s, 3);

    trs_hset_drop(s);
}

/* ========== remove ========== */

static void test_remove_takes_the_key_out() {
    trs_HSet *s = make_filled(trs_hash_i32, 5);

    TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, s, 2));

    TEST_ASSERT_EQUAL_size_t(4, trs_hset_len(s));
    assert_missing(s, 2);
    assert_has(s, 1);
    assert_has(s, 3);

    trs_hset_drop(s);
}

static void test_remove_of_a_missing_key_says_so() {
    trs_HSet *s = make_filled(trs_hash_i32, 3);

    TEST_ASSERT_FALSE(TRS_HSET_REMOVE(int32_t, s, 99));
    TEST_ASSERT_EQUAL_size_t(3, trs_hset_len(s));

    trs_hset_drop(s);
}

static void test_remove_from_the_middle_of_a_chain() {
    trs_HSet *s = make_filled(hash_all_alike, 5);

    TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, s, 2));

    TEST_ASSERT_EQUAL_size_t(4, trs_hset_len(s));
    assert_missing(s, 2);
    for (int32_t i = 0; i < 5; ++i) {
        if (i != 2) {
            assert_has(s, i);
        }
    }

    trs_hset_drop(s);
}

// a set node carries no value, so the mutable walk is there to remove through: one pass
// takes out every even key and leaves the rest
static void test_mut_walk_removes_through_the_nodes() {
    trs_HSet *s = make_filled(trs_hash_i32, 6);

    trs_HSetNode *node = trs_hset_first_node_mut(s);
    while (node) {
        const int32_t key = *TRS_HSET_NODE_KEY_AS(int32_t, node);
        trs_HSetNode *next = trs_hset_node_next_mut(s, node);
        if (key % 2 == 0) {
            trs_hset_remove_node(s, node);
        }
        node = next;
    }

    TEST_ASSERT_EQUAL_size_t(3, trs_hset_len(s));
    assert_has(s, 1);
    assert_has(s, 3);
    assert_has(s, 5);
    assert_missing(s, 0);
    assert_missing(s, 2);
    assert_missing(s, 4);

    trs_hset_drop(s);
}

static void test_mut_walk_of_an_empty_set_stops_at_once() {
    trs_HSet *s = make_set(trs_hash_i32);

    TEST_ASSERT_NULL(trs_hset_first_node_mut(s));

    trs_hset_drop(s);
}

static void test_remove_node_drops_the_key_it_names() {
    trs_HSet *s = make_filled(hash_all_alike, 4);

    trs_HSetNode *node = trs_hset_find_mut(s, &(int32_t){2});
    TEST_ASSERT_NOT_NULL(node);

    trs_hset_remove_node(s, node);

    TEST_ASSERT_EQUAL_size_t(3, trs_hset_len(s));
    assert_missing(s, 2);
    assert_has(s, 3);

    trs_hset_drop(s);
}

static void test_a_key_can_be_put_back_after_removal() {
    trs_HSet *s = make_filled(trs_hash_i32, 3);

    TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, s, 1));
    put(s, 1);

    TEST_ASSERT_EQUAL_size_t(3, trs_hset_len(s));
    assert_has(s, 1);

    trs_hset_drop(s);
}

static void test_clear_empties_and_keeps_the_buckets() {
    trs_HSet *s = make_filled(trs_hash_i32, 20);
    const size_t buckets = trs_hset_bucket_count(s);

    trs_hset_clear(s);

    TEST_ASSERT_EQUAL_size_t(0, trs_hset_len(s));
    TEST_ASSERT_EQUAL_size_t(buckets, trs_hset_bucket_count(s));
    TEST_ASSERT_NULL(trs_hset_first_node(s));
    assert_missing(s, 1);

    trs_hset_drop(s);
}

static void test_clear_leaves_a_usable_set() {
    trs_HSet *s = make_filled(trs_hash_i32, 10);

    trs_hset_clear(s);
    put(s, 5);

    TEST_ASSERT_EQUAL_size_t(1, trs_hset_len(s));
    assert_has(s, 5);

    trs_hset_drop(s);
}

/* ========== walk ========== */

static void test_the_walk_reaches_every_key_once() {
    trs_HSet *s = make_filled(trs_hash_i32, 50);

    assert_walk_sees_everything(s);

    trs_hset_drop(s);
}

static void test_the_walk_crosses_a_single_chain() {
    trs_HSet *s = make_filled(hash_all_alike, 12);

    assert_walk_sees_everything(s);

    trs_hset_drop(s);
}

static void test_the_walk_after_a_removal_sees_the_rest() {
    trs_HSet *s = make_filled(trs_hash_i32, 30);

    for (int32_t i = 0; i < 30; i += 2) {
        TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, s, i));
    }

    TEST_ASSERT_EQUAL_size_t(15, trs_hset_len(s));
    assert_walk_sees_everything(s);

    trs_hset_drop(s);
}

static void test_shrink_to_fit_gives_the_buckets_back() {
    trs_HSet *s = make_filled(trs_hash_i32, 300);
    const size_t grown = trs_hset_bucket_count(s);

    for (int32_t i = 3; i < 300; ++i) {
        TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, s, i));
    }

    TRS_TEST_OK(trs_hset_shrink_to_fit(s));

    TEST_ASSERT_TRUE(trs_hset_bucket_count(s) < grown);
    TEST_ASSERT_TRUE(trs_hset_bucket_count(s) >= trs_hset_len(s));
    for (int32_t i = 0; i < 3; ++i) {
        assert_has(s, i);
    }
    assert_walk_sees_everything(s);

    trs_hset_drop(s);
}

static void test_shrink_to_fit_of_an_empty_set_owns_nothing() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, &al, &s));
    for (int32_t i = 0; i < 50; ++i) {
        put(s, i);
    }
    trs_hset_clear(s);

    TRS_TEST_OK(trs_hset_shrink_to_fit(s));

    TEST_ASSERT_EQUAL_size_t(0, trs_hset_bucket_count(s));
    TEST_ASSERT_EQUAL_size_t(2, probe.live); // the set header and the map behind it

    put(s, 1); // still a set: the next key takes buckets again
    assert_has(s, 1);

    trs_hset_drop(s);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// shrinking relinks the nodes where they lie, so a borrowed one comes through it
static void test_shrink_to_fit_keeps_borrowed_nodes() {
    trs_HSet *s = make_filled(trs_hash_i32, 200);

    const trs_HSetNode *held = TRS_HSET_FIND(int32_t, s, 1);
    TEST_ASSERT_NOT_NULL(held);

    for (int32_t i = 3; i < 200; ++i) {
        TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, s, i));
    }
    TRS_TEST_OK(trs_hset_shrink_to_fit(s));

    TEST_ASSERT_EQUAL_PTR(held, TRS_HSET_FIND(int32_t, s, 1));
    TEST_ASSERT_EQUAL_INT32(1, *TRS_HSET_NODE_KEY_AS(int32_t, held));

    trs_hset_drop(s);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    trs_HSet *src = make_filled(trs_hash_i32, 10);

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(trs_hset_copy(src, &dst));

    TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, dst, 1));
    put(dst, 100);

    assert_has(src, 1);
    assert_missing(src, 100);
    assert_missing(dst, 1);
    assert_has(dst, 100);

    trs_hset_drop(src);
    trs_hset_drop(dst);
}

static void test_copy_carries_the_hasher_and_the_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *src = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, hash_all_alike, trs_eq_i32, arena, &src));
    put(src, 1);

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(trs_hset_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(hash_all_alike, trs_hset_hasher(dst));
    TEST_ASSERT_EQUAL_PTR(arena, trs_hset_al(dst));
    assert_has(dst, 1);

    trs_hset_drop(src);
    trs_hset_drop(dst);
    trs_al_arena_drop(arena);
}

// a copy of a valueless map goes through the same private door the set was built with,
// so the clone keeps the small node too. The copy's own last block is the set header, so
// the size is read from the next key put into the clone instead
static void test_a_copy_keeps_the_smaller_node() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_HSet *src = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, &al, &src));
    TRS_TEST_OK(TRS_HSET_INSERT(int32_t, src, 1, nullptr));
    const size_t node = probe.last_alloc_size;

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(trs_hset_copy(src, &dst));
    assert_has(dst, 1);

    TRS_TEST_OK(TRS_HSET_INSERT(int32_t, dst, 2, nullptr)); // fits the buckets already there
    TEST_ASSERT_EQUAL_size_t(node, probe.last_alloc_size);

    trs_hset_drop(src);
    trs_hset_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_copy_with_builds_on_the_given_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *src = make_filled(trs_hash_i32, 8);

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(trs_hset_copy_with(src, arena, &dst));

    TEST_ASSERT_EQUAL_PTR(arena, trs_hset_al(dst));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_hset_al(src));
    TEST_ASSERT_EQUAL_PTR(trs_hash_i32, trs_hset_hasher(dst));
    TEST_ASSERT_EQUAL_PTR(trs_eq_i32, trs_hset_key_eq(dst));
    TEST_ASSERT_TRUE(trs_hset_eq(src, dst));

    // the source is gone and the copy still answers: the nodes are its own
    trs_hset_drop(src);
    assert_has(dst, 3);

    trs_hset_drop(dst);
    trs_al_arena_drop(arena);
}

// the buckets and the nodes are asked of the allocator the copy is going to
static void test_copy_with_reports_an_exhausted_target_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_HSet *src = make_filled(trs_hash_i32, 8);

    trs_HSet *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_hset_copy_with(src, arena, &dst));
    TEST_ASSERT_NULL(dst);
    TEST_ASSERT_EQUAL_size_t(8, trs_hset_len(src));

    trs_hset_drop(src);
    trs_al_arena_drop(arena);
}

static void test_move_assign_hands_over_the_contents_on_one_allocator() {
    trs_TestProbe probe;
    trs_test_probe_reset(&probe);
    trs_Al al = trs_test_probe_full(&probe);

    trs_HSet *src = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, hash_all_alike, trs_eq_i32, &al, &src));
    put(src, 1);
    put(src, 2);

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, &al, &dst));
    put(dst, 9);

    const size_t requests = trs_test_probe_requests(&probe);
    TRS_TEST_OK(trs_hset_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(requests, trs_test_probe_requests(&probe));

    TEST_ASSERT_EQUAL_size_t(2, trs_hset_len(dst));
    assert_has(dst, 1);
    assert_has(dst, 2);
    TEST_ASSERT_EQUAL_PTR(hash_all_alike, trs_hset_hasher(dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_hset_len(src));

    trs_hset_drop(src);
    trs_hset_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_move_assign_across_allocators_empties_the_source() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *src = make_filled(trs_hash_i32, 8);

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, hash_all_alike, trs_eq_i32, arena, &dst));
    put(dst, 9);

    TRS_TEST_OK(trs_hset_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(8, trs_hset_len(dst));
    assert_has(dst, 3);
    assert_missing(dst, 9);
    TEST_ASSERT_EQUAL_PTR(trs_hash_i32, trs_hset_hasher(dst));
    TEST_ASSERT_EQUAL_PTR(arena, trs_hset_al(dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_hset_len(src));
    TEST_ASSERT_EQUAL_PTR(trs_al_default(), trs_hset_al(src));

    trs_hset_drop(src);
    trs_hset_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_across_allocators_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, arena, &dst));
    put(dst, 9);
    trs_test_arena_leave(arena, 0);

    trs_HSet *src = make_filled(trs_hash_i32, 8);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_hset_move_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(8, trs_hset_len(src));
    TEST_ASSERT_EQUAL_size_t(1, trs_hset_len(dst));
    assert_has(dst, 9);

    trs_hset_drop(src);
    trs_hset_drop(dst);
    trs_al_arena_drop(arena);
}

static void test_move_assign_of_itself_changes_nothing() {
    trs_HSet *s = make_filled(trs_hash_i32, 4);

    TRS_TEST_OK(trs_hset_move_assign(s, s));

    TEST_ASSERT_EQUAL_size_t(4, trs_hset_len(s));
    assert_has(s, 2);

    trs_hset_drop(s);
}

static void test_copy_of_empty_stays_empty() {
    trs_HSet *src = make_set(trs_hash_i32);

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(trs_hset_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(0, trs_hset_len(dst));

    trs_hset_drop(src);
    trs_hset_drop(dst);
}

static void test_copy_assign_overwrites_the_target() {
    trs_HSet *src = make_filled(trs_hash_i32, 6);
    trs_HSet *dst = make_filled(trs_hash_i32, 2);
    put(dst, 100);

    TRS_TEST_OK(trs_hset_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(6, trs_hset_len(dst));
    assert_missing(dst, 100);
    for (int32_t i = 0; i < 6; ++i) {
        assert_has(dst, i);
    }

    trs_hset_drop(src);
    trs_hset_drop(dst);
}

static void test_copy_assign_self_is_noop() {
    trs_HSet *s = make_filled(trs_hash_i32, 5);

    TRS_TEST_OK(trs_hset_copy_assign(s, s));

    TEST_ASSERT_EQUAL_size_t(5, trs_hset_len(s));
    assert_has(s, 3);

    trs_hset_drop(s);
}

/* ========== swap ========== */

static void test_swap_exchanges_the_keys_and_the_hashers() {
    trs_HSet *a = make_set(trs_hash_i32);
    put(a, 1);

    trs_HSet *b = make_set(hash_all_alike);
    put(b, 2);
    put(b, 3);

    trs_hset_swap(a, b);

    TEST_ASSERT_EQUAL_size_t(2, trs_hset_len(a));
    TEST_ASSERT_EQUAL_size_t(1, trs_hset_len(b));
    TEST_ASSERT_EQUAL_PTR(hash_all_alike, trs_hset_hasher(a));
    TEST_ASSERT_EQUAL_PTR(trs_hash_i32, trs_hset_hasher(b));
    assert_has(a, 2);
    assert_has(b, 1);

    trs_hset_drop(a);
    trs_hset_drop(b);
}

static void test_swap_keeps_the_nodes_alive() {
    trs_HSet *a = make_filled(trs_hash_i32, 4);
    trs_HSet *b = make_set(trs_hash_i32);

    const trs_HSetNode *held = TRS_HSET_FIND(int32_t, a, 2);
    TEST_ASSERT_NOT_NULL(held);

    trs_hset_swap(a, b);

    TEST_ASSERT_EQUAL_PTR(held, TRS_HSET_FIND(int32_t, b, 2));

    trs_hset_drop(a);
    trs_hset_drop(b);
}

static void test_swap_self_is_noop() {
    trs_HSet *s = make_filled(trs_hash_i32, 3);

    trs_hset_swap(s, s);

    TEST_ASSERT_EQUAL_size_t(3, trs_hset_len(s));
    assert_has(s, 1);

    trs_hset_drop(s);
}

/* ========== allocation failure ========== */

static void test_new_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    trs_test_arena_leave(arena, 0);

    trs_HSet *s = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, arena, &s));
    TEST_ASSERT_NULL(s);

    trs_al_arena_drop(arena);
}

static void test_insert_reports_an_exhausted_arena_and_changes_nothing() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, arena, &s));
    put(s, 1);
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, TRS_HSET_INSERT(int32_t, s, 2, nullptr));

    TEST_ASSERT_EQUAL_size_t(1, trs_hset_len(s));
    assert_has(s, 1);
    assert_missing(s, 2);

    trs_al_arena_drop(arena);
}

// putting a key that is already there needs no memory, so an exhausted arena is no
// obstacle to it
static void test_a_repeat_insert_needs_no_allocator() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, arena, &s));
    put(s, 1);
    trs_test_arena_leave(arena, 0);

    bool is_new = true;
    TRS_TEST_OK(TRS_HSET_INSERT(int32_t, s, 1, &is_new));
    TEST_ASSERT_FALSE(is_new);

    trs_al_arena_drop(arena);
}

static void test_reserve_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *s = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, arena, &s));
    put(s, 1);
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_hset_reserve(s, 100000));

    TEST_ASSERT_EQUAL_size_t(1, trs_hset_len(s));
    assert_has(s, 1);

    trs_al_arena_drop(arena);
}

static void test_copy_reports_an_exhausted_arena() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *src = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, arena, &src));
    put(src, 1);
    trs_test_arena_leave(arena, 0);

    trs_HSet *dst = nullptr;
    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_hset_copy(src, &dst));
    TEST_ASSERT_NULL(dst);

    trs_al_arena_drop(arena);
}

static void test_copy_assign_leaves_the_target_untouched_on_failure() {
    trs_Al *arena = trs_al_arena_new(trs_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    trs_HSet *src = make_filled(trs_hash_i32, 50);

    trs_HSet *dst = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW(int32_t, trs_hash_i32, trs_eq_i32, arena, &dst));
    put(dst, 7);
    trs_test_arena_leave(arena, 0);

    TRS_TEST_STATUS(TRS_STATUS_ERR_NO_MEM, trs_hset_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(1, trs_hset_len(dst));
    assert_has(dst, 7);

    trs_hset_drop(src);
    trs_al_arena_drop(arena);
}

/* ========== compare ========== */

static void test_eq_ignores_insertion_order_and_bucket_count() {
    trs_HSet *a = make_filled(trs_hash_i32, 40);

    trs_HSet *b = nullptr;
    TRS_TEST_OK(TRS_HSET_NEW_CAP(int32_t, 256, trs_hash_i32, trs_eq_i32, trs_al_default(), &b));
    for (int32_t i = 39; i >= 0; --i) {
        put(b, i);
    }

    TEST_ASSERT_TRUE(trs_hset_bucket_count(a) != trs_hset_bucket_count(b));
    TEST_ASSERT_TRUE(trs_hset_eq(a, a));
    TEST_ASSERT_TRUE(trs_hset_eq(a, b));
    TEST_ASSERT_TRUE(trs_hset_eq(b, a));

    trs_hset_drop(a);
    trs_hset_drop(b);
}

// the keys of 'a' are looked up in 'b', so the hasher that answers is the one of 'b'
static void test_eq_looks_the_keys_up_with_the_hasher_of_the_other() {
    trs_HSet *a = make_filled(trs_hash_i32, 12);
    trs_HSet *b = make_filled(hash_all_alike, 12);

    TEST_ASSERT_TRUE(trs_hset_eq(a, b));
    TEST_ASSERT_TRUE(trs_hset_eq(b, a));

    trs_hset_drop(a);
    trs_hset_drop(b);
}

// the same number of keys, one in place of another
static void test_eq_parts_a_differing_key() {
    trs_HSet *a = make_filled(trs_hash_i32, 8);
    trs_HSet *b = make_filled(trs_hash_i32, 8);
    TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, b, 3));
    put(b, 100);

    TEST_ASSERT_EQUAL_size_t(trs_hset_len(a), trs_hset_len(b));
    TEST_ASSERT_FALSE(trs_hset_eq(a, b));
    TEST_ASSERT_FALSE(trs_hset_eq(b, a));

    trs_hset_drop(a);
    trs_hset_drop(b);
}

// one is a proper subset of the other, so only the length says no
static void test_eq_parts_different_lengths() {
    trs_HSet *a = make_filled(trs_hash_i32, 8);
    trs_HSet *smaller = make_filled(trs_hash_i32, 7);

    TEST_ASSERT_FALSE(trs_hset_eq(a, smaller));
    TEST_ASSERT_FALSE(trs_hset_eq(smaller, a));

    trs_hset_drop(a);
    trs_hset_drop(smaller);
}

static void test_eq_of_two_empties() {
    trs_HSet *a = make_set(trs_hash_i32);
    trs_HSet *b = make_filled(trs_hash_i32, 8);
    trs_HSet *one = make_filled(trs_hash_i32, 1);

    trs_hset_clear(b);

    TEST_ASSERT_TRUE(trs_hset_eq(a, b));
    TEST_ASSERT_TRUE(trs_hset_eq(b, a));
    TEST_ASSERT_FALSE(trs_hset_eq(a, one));

    trs_hset_drop(a);
    trs_hset_drop(b);
    trs_hset_drop(one);
}

static void test_eq_walks_whole_chains() {
    trs_HSet *a = make_filled(hash_all_alike, 16);
    trs_HSet *b = make_filled(hash_all_alike, 16);
    TEST_ASSERT_TRUE(TRS_HSET_REMOVE(int32_t, b, 15));
    put(b, 100);

    TEST_ASSERT_TRUE(trs_hset_eq(a, a));
    TEST_ASSERT_FALSE(trs_hset_eq(a, b));

    trs_hset_drop(a);
    trs_hset_drop(b);
}

static void test_eq_matches_a_copy() {
    trs_HSet *a = make_filled(trs_hash_i32, 24);

    trs_HSet *copy = nullptr;
    TRS_TEST_OK(trs_hset_copy(a, &copy));

    TEST_ASSERT_TRUE(trs_hset_eq(a, copy));

    trs_hset_drop(a);
    trs_hset_drop(copy);
}

/* ========== print ========== */

// a printer writes to a stream, so a case has to read one back. tmpfile is the portable
// way, the same one test/core/test_print.c takes
static void assert_prints(const char *expected, const trs_HSet *s) {
    FILE *stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);

    trs_hset_fprint(s, stream, trs_fprint_i32);
    rewind(stream);

    char buf[128];
    const size_t n = fread(buf, 1, sizeof buf - 1, stream);
    buf[n] = '\0';
    fclose(stream);

    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

static void test_fprint_writes_the_key() {
    trs_HSet *s = make_set(trs_hash_i32);
    put(s, 1);

    assert_prints("{1}\n", s);

    trs_hset_drop(s);
}

// with more than one key the order is the buckets', which the header calls unspecified —
// so a case may say what is printed only where there is nothing to order
static void test_fprint_of_an_empty_set() {
    trs_HSet *s = make_set(trs_hash_i32);

    assert_prints("{}\n", s);

    trs_hset_drop(s);
}

// the stdout twin takes no stream, and C has no portable way to capture one and give it
// back — so a case can only say that it runs and reaches the same printer
static void test_print_writes_to_stdout() {
    trs_HSet *s = make_set(trs_hash_i32);
    put(s, 1);
    put(s, 2);

    trs_hset_print(s, trs_fprint_i32);

    trs_hset_drop(s);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_a_set_node_is_a_map_node_without_the_value);

    RUN_TEST(test_new_starts_empty);
    RUN_TEST(test_new_cap_reserves_buckets_without_keys);
    RUN_TEST(test_drop_null_is_noop);
    RUN_TEST(test_drop_hands_back_everything);
    RUN_TEST(test_a_refused_header_frees_the_map);

    RUN_TEST(test_insert_then_contains);
    RUN_TEST(test_contains_on_an_empty_set_is_false);
    RUN_TEST(test_inserting_a_key_twice_keeps_one_entry);
    RUN_TEST(test_insert_reports_whether_the_key_was_new);
    RUN_TEST(test_find_gives_the_key_back);
    RUN_TEST(test_a_set_whose_keys_all_collide_still_finds_them);
    RUN_TEST(test_wide_keys_travel_whole);

    RUN_TEST(test_every_key_survives_the_growths);
    RUN_TEST(test_a_borrowed_node_survives_every_growth);
    RUN_TEST(test_reserve_grows_the_buckets_only);
    RUN_TEST(test_shrink_to_fit_gives_the_buckets_back);
    RUN_TEST(test_shrink_to_fit_of_an_empty_set_owns_nothing);
    RUN_TEST(test_shrink_to_fit_keeps_borrowed_nodes);

    RUN_TEST(test_remove_takes_the_key_out);
    RUN_TEST(test_remove_of_a_missing_key_says_so);
    RUN_TEST(test_remove_from_the_middle_of_a_chain);
    RUN_TEST(test_mut_walk_removes_through_the_nodes);
    RUN_TEST(test_mut_walk_of_an_empty_set_stops_at_once);
    RUN_TEST(test_remove_node_drops_the_key_it_names);
    RUN_TEST(test_a_key_can_be_put_back_after_removal);
    RUN_TEST(test_clear_empties_and_keeps_the_buckets);
    RUN_TEST(test_clear_leaves_a_usable_set);

    RUN_TEST(test_the_walk_reaches_every_key_once);
    RUN_TEST(test_the_walk_crosses_a_single_chain);
    RUN_TEST(test_the_walk_after_a_removal_sees_the_rest);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_carries_the_hasher_and_the_allocator);
    RUN_TEST(test_copy_with_builds_on_the_given_allocator);
    RUN_TEST(test_copy_with_reports_an_exhausted_target_arena);
    RUN_TEST(test_move_assign_hands_over_the_contents_on_one_allocator);
    RUN_TEST(test_move_assign_across_allocators_empties_the_source);
    RUN_TEST(test_move_assign_across_allocators_reports_an_exhausted_arena);
    RUN_TEST(test_move_assign_of_itself_changes_nothing);
    RUN_TEST(test_a_copy_keeps_the_smaller_node);
    RUN_TEST(test_copy_of_empty_stays_empty);
    RUN_TEST(test_copy_assign_overwrites_the_target);
    RUN_TEST(test_copy_assign_self_is_noop);

    RUN_TEST(test_swap_exchanges_the_keys_and_the_hashers);
    RUN_TEST(test_swap_keeps_the_nodes_alive);
    RUN_TEST(test_swap_self_is_noop);

    RUN_TEST(test_new_reports_an_exhausted_arena);
    RUN_TEST(test_insert_reports_an_exhausted_arena_and_changes_nothing);
    RUN_TEST(test_a_repeat_insert_needs_no_allocator);
    RUN_TEST(test_reserve_reports_an_exhausted_arena);
    RUN_TEST(test_copy_reports_an_exhausted_arena);
    RUN_TEST(test_copy_assign_leaves_the_target_untouched_on_failure);


    RUN_TEST(test_eq_ignores_insertion_order_and_bucket_count);
    RUN_TEST(test_eq_looks_the_keys_up_with_the_hasher_of_the_other);
    RUN_TEST(test_eq_parts_a_differing_key);
    RUN_TEST(test_eq_parts_different_lengths);
    RUN_TEST(test_eq_of_two_empties);
    RUN_TEST(test_eq_walks_whole_chains);
    RUN_TEST(test_eq_matches_a_copy);

    RUN_TEST(test_fprint_writes_the_key);
    RUN_TEST(test_fprint_of_an_empty_set);
    RUN_TEST(test_print_writes_to_stdout);

    return UNITY_END();
}
