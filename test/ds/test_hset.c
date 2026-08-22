#include "nad/ds/hset.h"
#include "nad/alloc/arena.h"
#include "nad/alloc/default.h"
#include "nad/core/cmp.h"
#include "nad/core/hash.h"
#include "nad/core/util.h"

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

// every key in one bucket, so the chain is walked instead of hit at its head
static nad_Hash hash_all_alike(const void *x) {
    NAD_UNUSED(x);

    return 0;
}

static nad_Hash hash_pair(const void *x) {
    const Pair *p = x;

    return nad_hash_combine(nad_hash_i64(&p->a), nad_hash_i64(&p->b));
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
static nad_HSet *make_set(nad_Hasher hasher) {
    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, hasher, nad_eq_i32, nad_al_default(), &s));

    return s;
}

static void put(nad_HSet *s, int32_t key) {
    NAD_TEST_OK(NAD_HSET_INSERT(int32_t, s, key, nullptr));
}

[[nodiscard]]
static nad_HSet *make_filled(nad_Hasher hasher, int32_t n) {
    nad_HSet *s = make_set(hasher);
    for (int32_t i = 0; i < n; ++i) {
        put(s, i);
    }
    return s;
}

static void assert_has(const nad_HSet *s, int32_t key) {
    TEST_ASSERT_TRUE(NAD_HSET_CONTAINS(int32_t, s, key));
    TEST_ASSERT_NOT_NULL(NAD_HSET_FIND(int32_t, s, key));
}

static void assert_missing(const nad_HSet *s, int32_t key) {
    TEST_ASSERT_FALSE(NAD_HSET_CONTAINS(int32_t, s, key));
    TEST_ASSERT_NULL(NAD_HSET_FIND(int32_t, s, key));
}

// the walk reaches exactly 'len' keys and every one of them is in the set
static void assert_walk_sees_everything(const nad_HSet *s) {
    size_t seen = 0;
    for (const nad_HSetNode *node = nad_hset_first_node(s); node; node = nad_hset_node_next(s, node)) {
        assert_has(s, *NAD_HSET_NODE_KEY_AS(int32_t, node));
        ++seen;
    }
    TEST_ASSERT_EQUAL_size_t(nad_hset_len(s), seen);
}

/* ========== the node layout ========== */

// the whole reason ds/hset goes through internal/hmap_impl.h rather than storing a dummy
// byte: with no value behind the key there is nothing to pad the key to, so a set's node
// is a map's node minus the value AND minus the padding that would have aligned it
static void test_a_set_node_is_a_map_node_without_the_value() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, &al, &s));
    NAD_TEST_OK(NAD_HSET_INSERT(int32_t, s, 1, nullptr));
    const size_t set_node = probe.last_alloc_size; // the node is the last block a first insert takes

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, &al, &m));
    NAD_TEST_OK(NAD_HMAP_INSERT(int32_t, int32_t, m, 1, 10, nullptr));
    const size_t map_node = probe.last_alloc_size;

    const size_t padding = align_up_to(sizeof(int32_t), alignof(max_align_t)) - sizeof(int32_t);
    TEST_ASSERT_TRUE(set_node < map_node);
    TEST_ASSERT_EQUAL_size_t(set_node + padding + sizeof(int32_t), map_node);

    nad_hset_drop(s);
    nad_hmap_drop(m);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== lifetime ========== */

static void test_new_starts_empty() {
    nad_HSet *s = make_set(nad_hash_i32);

    TEST_ASSERT_EQUAL_size_t(0, nad_hset_len(s));
    TEST_ASSERT_EQUAL_size_t(0, nad_hset_bucket_count(s));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_hset_key_size(s));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_hset_al(s));
    TEST_ASSERT_EQUAL_PTR(nad_hash_i32, nad_hset_hasher(s));
    TEST_ASSERT_EQUAL_PTR(nad_eq_i32, nad_hset_eq(s));
    TEST_ASSERT_NULL(nad_hset_first_node(s));

    nad_hset_drop(s);
}

static void test_new_cap_reserves_buckets_without_keys() {
    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW_CAP(int32_t, 100, nad_hash_i32, nad_eq_i32, nad_al_default(), &s));

    TEST_ASSERT_EQUAL_size_t(0, nad_hset_len(s));
    TEST_ASSERT_TRUE(nad_hset_bucket_count(s) >= 100);

    nad_hset_drop(s);
}

static void test_drop_null_is_noop() {
    nad_hset_drop(nullptr);
}

// three blocks go into a filled set — the set header, the map behind it, and the buckets —
// plus one per key, and drop must hand back all of them
static void test_drop_hands_back_everything() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, &al, &s));
    for (int32_t i = 0; i < 20; ++i) {
        put(s, i);
    }

    nad_hset_drop(s);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the set header is taken after the map, and a refusal of it must not strand the map
static void test_a_refused_header_frees_the_map() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_test_probe_fail_after_next(&probe, 1);

    nad_HSet *s = nullptr;
    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, &al, &s)
    );

    TEST_ASSERT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== insert and lookup ========== */

static void test_insert_then_contains() {
    nad_HSet *s = make_set(nad_hash_i32);

    put(s, 1);
    put(s, 2);

    assert_has(s, 1);
    assert_has(s, 2);
    assert_missing(s, 3);
    TEST_ASSERT_EQUAL_size_t(2, nad_hset_len(s));

    nad_hset_drop(s);
}

static void test_contains_on_an_empty_set_is_false() {
    nad_HSet *s = make_set(nad_hash_i32);

    assert_missing(s, 1);

    nad_hset_drop(s);
}

// a set holds a key once however many times it is put in
static void test_inserting_a_key_twice_keeps_one_entry() {
    nad_HSet *s = make_set(nad_hash_i32);

    put(s, 1);
    put(s, 1);
    put(s, 1);

    TEST_ASSERT_EQUAL_size_t(1, nad_hset_len(s));
    assert_has(s, 1);

    nad_hset_drop(s);
}

static void test_insert_reports_whether_the_key_was_new() {
    nad_HSet *s = make_set(nad_hash_i32);

    bool is_new = false;
    NAD_TEST_OK(NAD_HSET_INSERT(int32_t, s, 1, &is_new));
    TEST_ASSERT_TRUE(is_new);

    NAD_TEST_OK(NAD_HSET_INSERT(int32_t, s, 1, &is_new));
    TEST_ASSERT_FALSE(is_new);

    NAD_TEST_OK(NAD_HSET_INSERT(int32_t, s, 2, &is_new));
    TEST_ASSERT_TRUE(is_new);

    nad_hset_drop(s);
}

static void test_find_gives_the_key_back() {
    nad_HSet *s = make_filled(nad_hash_i32, 5);

    const nad_HSetNode *node = NAD_HSET_FIND(int32_t, s, 3);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT32(3, *NAD_HSET_NODE_KEY_AS(int32_t, node));

    nad_hset_drop(s);
}

static void test_a_set_whose_keys_all_collide_still_finds_them() {
    nad_HSet *s = make_filled(hash_all_alike, 20);

    TEST_ASSERT_EQUAL_size_t(20, nad_hset_len(s));
    for (int32_t i = 0; i < 20; ++i) {
        assert_has(s, i);
    }
    assert_missing(s, 20);

    nad_hset_drop(s);
}

static void test_wide_keys_travel_whole() {
    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(Pair, hash_pair, eq_pair, nad_al_default(), &s));

    constexpr Pair a = {1, 2};
    constexpr Pair b = {1, 3};
    NAD_TEST_OK(nad_hset_insert(s, &a, nullptr));

    TEST_ASSERT_TRUE(nad_hset_contains(s, &a));
    TEST_ASSERT_FALSE(nad_hset_contains(s, &b)); // the second field is part of the key
    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), nad_hset_key_size(s));

    nad_hset_drop(s);
}

/* ========== growth ========== */

static void test_every_key_survives_the_growths() {
    nad_HSet *s = make_filled(nad_hash_i32, 300);

    TEST_ASSERT_EQUAL_size_t(300, nad_hset_len(s));
    for (int32_t i = 0; i < 300; ++i) {
        assert_has(s, i);
    }
    assert_missing(s, 300);

    nad_hset_drop(s);
}

static void test_a_borrowed_node_survives_every_growth() {
    nad_HSet *s = make_filled(nad_hash_i32, 4);

    const nad_HSetNode *held[4];
    for (int32_t i = 0; i < 4; ++i) {
        held[i] = NAD_HSET_FIND(int32_t, s, i);
        TEST_ASSERT_NOT_NULL(held[i]);
    }

    for (int32_t i = 4; i < 500; ++i) {
        put(s, i);
    }

    for (int32_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_PTR(held[i], NAD_HSET_FIND(int32_t, s, i));
        TEST_ASSERT_EQUAL_INT32(i, *NAD_HSET_NODE_KEY_AS(int32_t, held[i]));
    }

    nad_hset_drop(s);
}

static void test_reserve_grows_the_buckets_only() {
    nad_HSet *s = make_filled(nad_hash_i32, 4);

    NAD_TEST_OK(nad_hset_reserve(s, 1000));

    TEST_ASSERT_TRUE(nad_hset_bucket_count(s) >= 1000);
    TEST_ASSERT_EQUAL_size_t(4, nad_hset_len(s));
    assert_has(s, 3);

    nad_hset_drop(s);
}

/* ========== remove ========== */

static void test_remove_takes_the_key_out() {
    nad_HSet *s = make_filled(nad_hash_i32, 5);

    TEST_ASSERT_TRUE(NAD_HSET_REMOVE(int32_t, s, 2));

    TEST_ASSERT_EQUAL_size_t(4, nad_hset_len(s));
    assert_missing(s, 2);
    assert_has(s, 1);
    assert_has(s, 3);

    nad_hset_drop(s);
}

static void test_remove_of_a_missing_key_says_so() {
    nad_HSet *s = make_filled(nad_hash_i32, 3);

    TEST_ASSERT_FALSE(NAD_HSET_REMOVE(int32_t, s, 99));
    TEST_ASSERT_EQUAL_size_t(3, nad_hset_len(s));

    nad_hset_drop(s);
}

static void test_remove_from_the_middle_of_a_chain() {
    nad_HSet *s = make_filled(hash_all_alike, 5);

    TEST_ASSERT_TRUE(NAD_HSET_REMOVE(int32_t, s, 2));

    TEST_ASSERT_EQUAL_size_t(4, nad_hset_len(s));
    assert_missing(s, 2);
    for (int32_t i = 0; i < 5; ++i) {
        if (i != 2) {
            assert_has(s, i);
        }
    }

    nad_hset_drop(s);
}

static void test_remove_node_drops_the_key_it_names() {
    nad_HSet *s = make_filled(hash_all_alike, 4);

    nad_HSetNode *node = nad_hset_find_mut(s, &(int32_t){2});
    TEST_ASSERT_NOT_NULL(node);

    nad_hset_remove_node(s, node);

    TEST_ASSERT_EQUAL_size_t(3, nad_hset_len(s));
    assert_missing(s, 2);
    assert_has(s, 3);

    nad_hset_drop(s);
}

static void test_a_key_can_be_put_back_after_removal() {
    nad_HSet *s = make_filled(nad_hash_i32, 3);

    TEST_ASSERT_TRUE(NAD_HSET_REMOVE(int32_t, s, 1));
    put(s, 1);

    TEST_ASSERT_EQUAL_size_t(3, nad_hset_len(s));
    assert_has(s, 1);

    nad_hset_drop(s);
}

static void test_clear_empties_and_keeps_the_buckets() {
    nad_HSet *s = make_filled(nad_hash_i32, 20);
    const size_t buckets = nad_hset_bucket_count(s);

    nad_hset_clear(s);

    TEST_ASSERT_EQUAL_size_t(0, nad_hset_len(s));
    TEST_ASSERT_EQUAL_size_t(buckets, nad_hset_bucket_count(s));
    TEST_ASSERT_NULL(nad_hset_first_node(s));
    assert_missing(s, 1);

    nad_hset_drop(s);
}

static void test_clear_leaves_a_usable_set() {
    nad_HSet *s = make_filled(nad_hash_i32, 10);

    nad_hset_clear(s);
    put(s, 5);

    TEST_ASSERT_EQUAL_size_t(1, nad_hset_len(s));
    assert_has(s, 5);

    nad_hset_drop(s);
}

/* ========== walk ========== */

static void test_the_walk_reaches_every_key_once() {
    nad_HSet *s = make_filled(nad_hash_i32, 50);

    assert_walk_sees_everything(s);

    nad_hset_drop(s);
}

static void test_the_walk_crosses_a_single_chain() {
    nad_HSet *s = make_filled(hash_all_alike, 12);

    assert_walk_sees_everything(s);

    nad_hset_drop(s);
}

static void test_the_walk_after_a_removal_sees_the_rest() {
    nad_HSet *s = make_filled(nad_hash_i32, 30);

    for (int32_t i = 0; i < 30; i += 2) {
        TEST_ASSERT_TRUE(NAD_HSET_REMOVE(int32_t, s, i));
    }

    TEST_ASSERT_EQUAL_size_t(15, nad_hset_len(s));
    assert_walk_sees_everything(s);

    nad_hset_drop(s);
}

static void test_shrink_to_fit_gives_the_buckets_back() {
    nad_HSet *s = make_filled(nad_hash_i32, 300);
    const size_t grown = nad_hset_bucket_count(s);

    for (int32_t i = 3; i < 300; ++i) {
        TEST_ASSERT_TRUE(NAD_HSET_REMOVE(int32_t, s, i));
    }

    NAD_TEST_OK(nad_hset_shrink_to_fit(s));

    TEST_ASSERT_TRUE(nad_hset_bucket_count(s) < grown);
    TEST_ASSERT_TRUE(nad_hset_bucket_count(s) >= nad_hset_len(s));
    for (int32_t i = 0; i < 3; ++i) {
        assert_has(s, i);
    }
    assert_walk_sees_everything(s);

    nad_hset_drop(s);
}

static void test_shrink_to_fit_of_an_empty_set_owns_nothing() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, &al, &s));
    for (int32_t i = 0; i < 50; ++i) {
        put(s, i);
    }
    nad_hset_clear(s);

    NAD_TEST_OK(nad_hset_shrink_to_fit(s));

    TEST_ASSERT_EQUAL_size_t(0, nad_hset_bucket_count(s));
    TEST_ASSERT_EQUAL_size_t(2, probe.live); // the set header and the map behind it

    put(s, 1); // still a set: the next key takes buckets again
    assert_has(s, 1);

    nad_hset_drop(s);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// shrinking relinks the nodes where they lie, so a borrowed one comes through it
static void test_shrink_to_fit_keeps_borrowed_nodes() {
    nad_HSet *s = make_filled(nad_hash_i32, 200);

    const nad_HSetNode *held = NAD_HSET_FIND(int32_t, s, 1);
    TEST_ASSERT_NOT_NULL(held);

    for (int32_t i = 3; i < 200; ++i) {
        TEST_ASSERT_TRUE(NAD_HSET_REMOVE(int32_t, s, i));
    }
    NAD_TEST_OK(nad_hset_shrink_to_fit(s));

    TEST_ASSERT_EQUAL_PTR(held, NAD_HSET_FIND(int32_t, s, 1));
    TEST_ASSERT_EQUAL_INT32(1, *NAD_HSET_NODE_KEY_AS(int32_t, held));

    nad_hset_drop(s);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    nad_HSet *src = make_filled(nad_hash_i32, 10);

    nad_HSet *dst = nullptr;
    NAD_TEST_OK(nad_hset_copy(src, &dst));

    TEST_ASSERT_TRUE(NAD_HSET_REMOVE(int32_t, dst, 1));
    put(dst, 100);

    assert_has(src, 1);
    assert_missing(src, 100);
    assert_missing(dst, 1);
    assert_has(dst, 100);

    nad_hset_drop(src);
    nad_hset_drop(dst);
}

static void test_copy_carries_the_hasher_and_the_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HSet *src = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, hash_all_alike, nad_eq_i32, arena, &src));
    put(src, 1);

    nad_HSet *dst = nullptr;
    NAD_TEST_OK(nad_hset_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(hash_all_alike, nad_hset_hasher(dst));
    TEST_ASSERT_EQUAL_PTR(arena, nad_hset_al(dst));
    assert_has(dst, 1);

    nad_hset_drop(src);
    nad_hset_drop(dst);
    nad_al_arena_drop(arena);
}

// a copy of a valueless map goes through the same private door the set was built with,
// so the clone keeps the small node too. The copy's own last block is the set header, so
// the size is read from the next key put into the clone instead
static void test_a_copy_keeps_the_smaller_node() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HSet *src = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, &al, &src));
    NAD_TEST_OK(NAD_HSET_INSERT(int32_t, src, 1, nullptr));
    const size_t node = probe.last_alloc_size;

    nad_HSet *dst = nullptr;
    NAD_TEST_OK(nad_hset_copy(src, &dst));
    assert_has(dst, 1);

    NAD_TEST_OK(NAD_HSET_INSERT(int32_t, dst, 2, nullptr)); // fits the buckets already there
    TEST_ASSERT_EQUAL_size_t(node, probe.last_alloc_size);

    nad_hset_drop(src);
    nad_hset_drop(dst);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_copy_of_empty_stays_empty() {
    nad_HSet *src = make_set(nad_hash_i32);

    nad_HSet *dst = nullptr;
    NAD_TEST_OK(nad_hset_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(0, nad_hset_len(dst));

    nad_hset_drop(src);
    nad_hset_drop(dst);
}

static void test_copy_assign_overwrites_the_target() {
    nad_HSet *src = make_filled(nad_hash_i32, 6);
    nad_HSet *dst = make_filled(nad_hash_i32, 2);
    put(dst, 100);

    NAD_TEST_OK(nad_hset_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(6, nad_hset_len(dst));
    assert_missing(dst, 100);
    for (int32_t i = 0; i < 6; ++i) {
        assert_has(dst, i);
    }

    nad_hset_drop(src);
    nad_hset_drop(dst);
}

static void test_copy_assign_self_is_noop() {
    nad_HSet *s = make_filled(nad_hash_i32, 5);

    NAD_TEST_OK(nad_hset_copy_assign(s, s));

    TEST_ASSERT_EQUAL_size_t(5, nad_hset_len(s));
    assert_has(s, 3);

    nad_hset_drop(s);
}

/* ========== swap ========== */

static void test_swap_exchanges_the_keys_and_the_hashers() {
    nad_HSet *a = make_set(nad_hash_i32);
    put(a, 1);

    nad_HSet *b = make_set(hash_all_alike);
    put(b, 2);
    put(b, 3);

    nad_hset_swap(a, b);

    TEST_ASSERT_EQUAL_size_t(2, nad_hset_len(a));
    TEST_ASSERT_EQUAL_size_t(1, nad_hset_len(b));
    TEST_ASSERT_EQUAL_PTR(hash_all_alike, nad_hset_hasher(a));
    TEST_ASSERT_EQUAL_PTR(nad_hash_i32, nad_hset_hasher(b));
    assert_has(a, 2);
    assert_has(b, 1);

    nad_hset_drop(a);
    nad_hset_drop(b);
}

static void test_swap_keeps_the_nodes_alive() {
    nad_HSet *a = make_filled(nad_hash_i32, 4);
    nad_HSet *b = make_set(nad_hash_i32);

    const nad_HSetNode *held = NAD_HSET_FIND(int32_t, a, 2);
    TEST_ASSERT_NOT_NULL(held);

    nad_hset_swap(a, b);

    TEST_ASSERT_EQUAL_PTR(held, NAD_HSET_FIND(int32_t, b, 2));

    nad_hset_drop(a);
    nad_hset_drop(b);
}

static void test_swap_self_is_noop() {
    nad_HSet *s = make_filled(nad_hash_i32, 3);

    nad_hset_swap(s, s);

    TEST_ASSERT_EQUAL_size_t(3, nad_hset_len(s));
    assert_has(s, 1);

    nad_hset_drop(s);
}

/* ========== allocation failure ========== */

static void test_new_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    nad_test_arena_leave(arena, 0);

    nad_HSet *s = nullptr;
    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, arena, &s)
    );
    TEST_ASSERT_NULL(s);

    nad_al_arena_drop(arena);
}

static void test_insert_reports_an_exhausted_arena_and_changes_nothing() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, arena, &s));
    put(s, 1);
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, NAD_HSET_INSERT(int32_t, s, 2, nullptr));

    TEST_ASSERT_EQUAL_size_t(1, nad_hset_len(s));
    assert_has(s, 1);
    assert_missing(s, 2);

    nad_al_arena_drop(arena);
}

// putting a key that is already there needs no memory, so an exhausted arena is no
// obstacle to it
static void test_a_repeat_insert_needs_no_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, arena, &s));
    put(s, 1);
    nad_test_arena_leave(arena, 0);

    bool is_new = true;
    NAD_TEST_OK(NAD_HSET_INSERT(int32_t, s, 1, &is_new));
    TEST_ASSERT_FALSE(is_new);

    nad_al_arena_drop(arena);
}

static void test_reserve_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HSet *s = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, arena, &s));
    put(s, 1);
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_hset_reserve(s, 100000));

    TEST_ASSERT_EQUAL_size_t(1, nad_hset_len(s));
    assert_has(s, 1);

    nad_al_arena_drop(arena);
}

static void test_copy_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HSet *src = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, arena, &src));
    put(src, 1);
    nad_test_arena_leave(arena, 0);

    nad_HSet *dst = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_hset_copy(src, &dst));
    TEST_ASSERT_NULL(dst);

    nad_al_arena_drop(arena);
}

static void test_copy_assign_leaves_the_target_untouched_on_failure() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HSet *src = make_filled(nad_hash_i32, 50);

    nad_HSet *dst = nullptr;
    NAD_TEST_OK(NAD_HSET_NEW(int32_t, nad_hash_i32, nad_eq_i32, arena, &dst));
    put(dst, 7);
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_hset_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(1, nad_hset_len(dst));
    assert_has(dst, 7);

    nad_hset_drop(src);
    nad_al_arena_drop(arena);
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
    RUN_TEST(test_remove_node_drops_the_key_it_names);
    RUN_TEST(test_a_key_can_be_put_back_after_removal);
    RUN_TEST(test_clear_empties_and_keeps_the_buckets);
    RUN_TEST(test_clear_leaves_a_usable_set);

    RUN_TEST(test_the_walk_reaches_every_key_once);
    RUN_TEST(test_the_walk_crosses_a_single_chain);
    RUN_TEST(test_the_walk_after_a_removal_sees_the_rest);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_carries_the_hasher_and_the_allocator);
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

    return UNITY_END();
}
