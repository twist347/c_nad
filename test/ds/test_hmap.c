#include "nad/ds/hmap.h"
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

// every key in one bucket. Without it the chains stay one node long and the code that
// walks them is never reached, whatever the keys are
static nad_Hash hash_all_alike(const void *x) {
    NAD_UNUSED(x);

    return 0;
}

// two keys per bucket at most, whatever the map's size: enough to build chains without
// collapsing the whole map into one
static nad_Hash hash_by_parity(const void *x) {
    return (nad_Hash) (*(const int32_t *) x & 1);
}

// a hasher and an equality for Pair, built the way core/hash prescribes: a struct's hash
// is folded from its fields' rather than taken over its bytes, since padding is not part
// of the value
static nad_Hash hash_pair(const void *x) {
    const Pair *p = x;

    return nad_hash_combine(nad_hash_i64(&p->a), nad_hash_i64(&p->b));
}

static bool eq_pair(const void *lhs, const void *rhs) {
    const Pair *a = lhs;
    const Pair *b = rhs;

    return a->a == b->a && a->b == b->b;
}

// hashes that differ in the high bits and agree in the low ones: every key lands in one
// bucket while no two hashes are equal
static nad_Hash hash_high_bits_only(const void *x) {
    return ((nad_Hash) *(const int32_t *) x) << 32;
}

static size_t hash_calls = 0;

static nad_Hash hash_i32_counting(const void *x) {
    ++hash_calls;

    return nad_hash_i32(x);
}

static size_t eq_calls = 0;

static bool eq_i32_counting(const void *lhs, const void *rhs) {
    ++eq_calls;

    return nad_eq_i32(lhs, rhs);
}

[[nodiscard]]
static nad_HMap *make_map(nad_Hasher hasher) {
    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, hasher, nad_eq_i32, nad_al_default(), &m));

    return m;
}

static void put(nad_HMap *m, int32_t key, int32_t val) {
    NAD_TEST_OK(NAD_HMAP_INSERT(int32_t, int32_t, m, key, val, nullptr));
}

// 'key * 10' under every key from 0 to n - 1
[[nodiscard]]
static nad_HMap *make_filled(nad_Hasher hasher, int32_t n) {
    nad_HMap *m = make_map(hasher);
    for (int32_t i = 0; i < n; ++i) {
        put(m, i, i * 10);
    }
    return m;
}

static void assert_has(const nad_HMap *m, int32_t key, int32_t val) {
    const int32_t *got = NAD_HMAP_GET_AS(int32_t, int32_t, m, key);
    TEST_ASSERT_NOT_NULL_MESSAGE(got, "the key is missing");
    TEST_ASSERT_EQUAL_INT32(val, *got);
    TEST_ASSERT_TRUE(NAD_HMAP_CONTAINS(int32_t, m, key));
}

static void assert_missing(const nad_HMap *m, int32_t key) {
    TEST_ASSERT_NULL(NAD_HMAP_GET_AS(int32_t, int32_t, m, key));
    TEST_ASSERT_FALSE(NAD_HMAP_CONTAINS(int32_t, m, key));
    TEST_ASSERT_NULL(NAD_HMAP_FIND(int32_t, m, key));
}

// the walk reaches exactly 'len' entries, and every one of them answers get with the
// value it carries. The order is unspecified, so that is all a walk can promise
static void assert_walk_sees_everything(const nad_HMap *m) {
    size_t seen = 0;
    for (const nad_HMapNode *node = nad_hmap_first_node(m); node; node = nad_hmap_node_next(m, node)) {
        const int32_t key = *NAD_HMAP_NODE_KEY_AS(int32_t, node);
        const int32_t val = *NAD_HMAP_NODE_VAL_AS(int32_t, m, node);
        assert_has(m, key, val);
        ++seen;
    }
    TEST_ASSERT_EQUAL_size_t(nad_hmap_len(m), seen);
}

/* ========== lifetime ========== */

static void test_new_starts_empty() {
    nad_HMap *m = make_map(nad_hash_i32);

    TEST_ASSERT_EQUAL_size_t(0, nad_hmap_len(m));
    TEST_ASSERT_EQUAL_size_t(0, nad_hmap_bucket_count(m));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_hmap_key_size(m));
    TEST_ASSERT_EQUAL_size_t(sizeof(int32_t), nad_hmap_val_size(m));
    TEST_ASSERT_EQUAL_PTR(nad_al_default(), nad_hmap_al(m));
    TEST_ASSERT_EQUAL_PTR(nad_hash_i32, nad_hmap_hasher(m));
    TEST_ASSERT_EQUAL_PTR(nad_eq_i32, nad_hmap_key_eq(m));
    TEST_ASSERT_NULL(nad_hmap_first_node(m));

    nad_hmap_drop(m);
}

// an empty map owns no buckets at all: the array is taken on the first insert
static void test_new_takes_no_buckets_until_the_first_insert() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, &al, &m));
    TEST_ASSERT_EQUAL_size_t(1, probe.live); // the header alone

    put(m, 1, 10);
    TEST_ASSERT_EQUAL_size_t(3, probe.live); // header, buckets, one node

    nad_hmap_drop(m);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_new_cap_reserves_buckets_without_entries() {
    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW_CAP(int32_t, int32_t, 100, nad_hash_i32, nad_eq_i32, nad_al_default(), &m));

    TEST_ASSERT_EQUAL_size_t(0, nad_hmap_len(m));
    TEST_ASSERT_TRUE(nad_hmap_bucket_count(m) >= 100);

    nad_hmap_drop(m);
}

static void test_drop_null_is_noop() {
    nad_hmap_drop(nullptr);
}

static void test_drop_hands_back_every_node() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, &al, &m));
    for (int32_t i = 0; i < 20; ++i) {
        put(m, i, i);
    }

    nad_hmap_drop(m);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== insert and lookup ========== */

static void test_insert_then_get() {
    nad_HMap *m = make_map(nad_hash_i32);

    put(m, 1, 10);
    put(m, 2, 20);

    assert_has(m, 1, 10);
    assert_has(m, 2, 20);
    TEST_ASSERT_EQUAL_size_t(2, nad_hmap_len(m));

    nad_hmap_drop(m);
}

static void test_get_of_a_missing_key_is_null() {
    nad_HMap *m = make_map(nad_hash_i32);
    put(m, 1, 10);

    assert_missing(m, 2);
    assert_missing(m, -1);

    nad_hmap_drop(m);
}

static void test_get_on_an_empty_map_is_null() {
    nad_HMap *m = make_map(nad_hash_i32);

    assert_missing(m, 1);
    TEST_ASSERT_EQUAL_size_t(0, nad_hmap_len(m));

    nad_hmap_drop(m);
}

static void test_insert_overwrites_an_existing_key() {
    nad_HMap *m = make_map(nad_hash_i32);

    put(m, 1, 10);
    put(m, 1, 99);

    assert_has(m, 1, 99);
    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(m)); // an overwrite is not a second entry

    nad_hmap_drop(m);
}

static void test_insert_reports_whether_the_key_was_new() {
    nad_HMap *m = make_map(nad_hash_i32);

    bool is_new = false;
    NAD_TEST_OK(NAD_HMAP_INSERT(int32_t, int32_t, m, 1, 10, &is_new));
    TEST_ASSERT_TRUE(is_new);

    NAD_TEST_OK(NAD_HMAP_INSERT(int32_t, int32_t, m, 1, 20, &is_new));
    TEST_ASSERT_FALSE(is_new);

    NAD_TEST_OK(NAD_HMAP_INSERT(int32_t, int32_t, m, 2, 20, &is_new));
    TEST_ASSERT_TRUE(is_new);

    nad_hmap_drop(m);
}

static void test_get_mut_writes_through() {
    nad_HMap *m = make_map(nad_hash_i32);
    put(m, 1, 10);

    *NAD_HMAP_GET_MUT_AS(int32_t, int32_t, m, 1) = 42;

    assert_has(m, 1, 42);

    nad_hmap_drop(m);
}

static void test_find_gives_the_entry_as_a_node() {
    nad_HMap *m = make_map(nad_hash_i32);
    put(m, 7, 70);

    const nad_HMapNode *node = NAD_HMAP_FIND(int32_t, m, 7);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT32(7, *NAD_HMAP_NODE_KEY_AS(int32_t, node));
    TEST_ASSERT_EQUAL_INT32(70, *NAD_HMAP_NODE_VAL_AS(int32_t, m, node));

    nad_hmap_drop(m);
}

static void test_node_val_mut_writes_through() {
    nad_HMap *m = make_map(nad_hash_i32);
    put(m, 7, 70);

    nad_HMapNode *node = nad_hmap_find_mut(m, &(int32_t){7});
    *NAD_HMAP_NODE_VAL_MUT_AS(int32_t, m, node) = 77;

    assert_has(m, 7, 77);

    nad_hmap_drop(m);
}

// every key lands in one bucket, so lookup has to walk a chain instead of hitting the
// head of it
static void test_a_map_whose_keys_all_collide_still_finds_them() {
    nad_HMap *m = make_filled(hash_all_alike, 20);

    TEST_ASSERT_EQUAL_size_t(20, nad_hmap_len(m));
    for (int32_t i = 0; i < 20; ++i) {
        assert_has(m, i, i * 10);
    }
    assert_missing(m, 20);

    nad_hmap_drop(m);
}

static void test_colliding_keys_overwrite_only_their_own_entry() {
    nad_HMap *m = make_filled(hash_all_alike, 5);

    put(m, 3, 999);

    TEST_ASSERT_EQUAL_size_t(5, nad_hmap_len(m));
    assert_has(m, 0, 0);
    assert_has(m, 3, 999);
    assert_has(m, 4, 40);

    nad_hmap_drop(m);
}

// a hash that agrees where the keys do not: 'eq' is what has the final word
static void test_a_hash_collision_is_not_an_equality() {
    nad_HMap *m = make_map(hash_all_alike);

    put(m, 1, 10);
    put(m, 2, 20);

    TEST_ASSERT_EQUAL_size_t(2, nad_hmap_len(m));
    assert_has(m, 1, 10);
    assert_has(m, 2, 20);

    nad_hmap_drop(m);
}

static void test_wide_keys_and_values_travel_whole() {
    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(Pair, Pair, hash_pair, eq_pair, nad_al_default(), &m));

    constexpr Pair key = {1, 2};
    constexpr Pair val = {30, 40};
    NAD_TEST_OK(nad_hmap_insert(m, &key, &val, nullptr));

    const Pair *got = nad_hmap_get(m, &key);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT64(30, got->a);
    TEST_ASSERT_EQUAL_INT64(40, got->b);
    TEST_ASSERT_EQUAL_size_t(sizeof(Pair), nad_hmap_key_size(m));

    nad_hmap_drop(m);
}

// what the eight bytes of cached hash buy: walking a chain of ten, only the node whose
// hash matches is ever handed to 'eq', and a miss asks it nothing at all
static void test_the_chain_is_walked_by_hash_before_eq() {
    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, hash_high_bits_only, eq_i32_counting, nad_al_default(), &m));

    for (int32_t i = 0; i < 10; ++i) {
        put(m, i, i * 10);
    }
    eq_calls = 0;
    const int32_t *got = NAD_HMAP_GET_AS(int32_t, int32_t, m, 7);
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_EQUAL_INT32(70, *got);
    TEST_ASSERT_EQUAL_size_t(1, eq_calls);

    eq_calls = 0;
    assert_missing(m, 100);
    TEST_ASSERT_EQUAL_size_t(0, eq_calls);

    nad_hmap_drop(m);
}

/* ========== growth ========== */

static void test_the_buckets_double_as_the_map_fills() {
    nad_HMap *m = make_map(nad_hash_i32);

    put(m, 0, 0);
    const size_t first = nad_hmap_bucket_count(m);
    TEST_ASSERT_TRUE(first > 0);

    for (int32_t i = 1; i < 200; ++i) {
        put(m, i, i);
    }

    TEST_ASSERT_TRUE(nad_hmap_bucket_count(m) > first);
    TEST_ASSERT_TRUE(nad_hmap_len(m) <= nad_hmap_bucket_count(m));

    nad_hmap_drop(m);
}

static void test_every_entry_survives_the_growths() {
    nad_HMap *m = make_filled(nad_hash_i32, 300);

    TEST_ASSERT_EQUAL_size_t(300, nad_hmap_len(m));
    for (int32_t i = 0; i < 300; ++i) {
        assert_has(m, i, i * 10);
    }
    assert_missing(m, 300);

    nad_hmap_drop(m);
}

// the property chaining was chosen for: growth reallocates the bucket array and relinks,
// so the node a caller borrowed before it is still that entry afterwards
static void test_a_borrowed_node_survives_every_growth() {
    nad_HMap *m = make_map(nad_hash_i32);

    for (int32_t i = 0; i < 4; ++i) {
        put(m, i, i * 10);
    }

    const nad_HMapNode *held[4];
    for (int32_t i = 0; i < 4; ++i) {
        held[i] = NAD_HMAP_FIND(int32_t, m, i);
        TEST_ASSERT_NOT_NULL(held[i]);
    }

    for (int32_t i = 4; i < 500; ++i) {
        put(m, i, i * 10);
    }

    for (int32_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_PTR(held[i], NAD_HMAP_FIND(int32_t, m, i));
        TEST_ASSERT_EQUAL_INT32(i, *NAD_HMAP_NODE_KEY_AS(int32_t, held[i]));
        TEST_ASSERT_EQUAL_INT32(i * 10, *NAD_HMAP_NODE_VAL_AS(int32_t, m, held[i]));
    }

    nad_hmap_drop(m);
}

// growing moves no entry, so the only allocation a rehash makes is the bucket array
static void test_a_growth_allocates_only_the_bucket_array() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW_CAP(int32_t, int32_t, 8, nad_hash_i32, nad_eq_i32, &al, &m));
    for (int32_t i = 0; i < 8; ++i) {
        put(m, i, i);
    }

    const size_t before = nad_test_probe_requests(&probe);
    put(m, 8, 8); // the one that pushes the load past one per bucket

    // one array plus one node, and not a single request per entry moved
    TEST_ASSERT_EQUAL_size_t(2, nad_test_probe_requests(&probe) - before);

    nad_hmap_drop(m);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_reserve_grows_the_buckets_only() {
    nad_HMap *m = make_filled(nad_hash_i32, 4);

    NAD_TEST_OK(nad_hmap_reserve(m, 1000));

    TEST_ASSERT_TRUE(nad_hmap_bucket_count(m) >= 1000);
    TEST_ASSERT_EQUAL_size_t(4, nad_hmap_len(m));
    for (int32_t i = 0; i < 4; ++i) {
        assert_has(m, i, i * 10);
    }

    nad_hmap_drop(m);
}

static void test_reserve_below_the_bucket_count_changes_nothing() {
    nad_HMap *m = make_filled(nad_hash_i32, 20);
    const size_t before = nad_hmap_bucket_count(m);

    NAD_TEST_OK(nad_hmap_reserve(m, 1));

    TEST_ASSERT_EQUAL_size_t(before, nad_hmap_bucket_count(m));

    nad_hmap_drop(m);
}

/* ========== remove ========== */

static void test_remove_takes_the_entry_out() {
    nad_HMap *m = make_filled(nad_hash_i32, 5);

    TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, 2));

    TEST_ASSERT_EQUAL_size_t(4, nad_hmap_len(m));
    assert_missing(m, 2);
    assert_has(m, 1, 10);
    assert_has(m, 3, 30);

    nad_hmap_drop(m);
}

static void test_remove_of_a_missing_key_says_so() {
    nad_HMap *m = make_filled(nad_hash_i32, 3);

    TEST_ASSERT_FALSE(NAD_HMAP_REMOVE(int32_t, m, 99));
    TEST_ASSERT_EQUAL_size_t(3, nad_hmap_len(m));

    nad_hmap_drop(m);
}

static void test_remove_on_an_empty_map_says_so() {
    nad_HMap *m = make_map(nad_hash_i32);

    TEST_ASSERT_FALSE(NAD_HMAP_REMOVE(int32_t, m, 1));

    nad_hmap_drop(m);
}

// unlinking from the middle of a chain is the case the head-of-bucket path never reaches
static void test_remove_from_the_middle_of_a_chain() {
    nad_HMap *m = make_filled(hash_all_alike, 5);

    TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, 2));

    TEST_ASSERT_EQUAL_size_t(4, nad_hmap_len(m));
    assert_missing(m, 2);
    for (int32_t i = 0; i < 5; ++i) {
        if (i != 2) {
            assert_has(m, i, i * 10);
        }
    }

    nad_hmap_drop(m);
}

static void test_remove_every_key_of_a_chain_in_turn() {
    nad_HMap *m = make_filled(hash_all_alike, 6);

    for (int32_t i = 0; i < 6; ++i) {
        TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, i));
        TEST_ASSERT_EQUAL_size_t((size_t) (5 - i), nad_hmap_len(m));
    }

    TEST_ASSERT_NULL(nad_hmap_first_node(m));

    nad_hmap_drop(m);
}

static void test_remove_node_drops_the_entry_it_names() {
    nad_HMap *m = make_filled(hash_all_alike, 4);

    nad_HMapNode *node = nad_hmap_find_mut(m, &(int32_t){2});
    TEST_ASSERT_NOT_NULL(node);

    nad_hmap_remove_node(m, node);

    TEST_ASSERT_EQUAL_size_t(3, nad_hmap_len(m));
    assert_missing(m, 2);
    assert_has(m, 3, 30);

    nad_hmap_drop(m);
}

static void test_a_key_can_be_put_back_after_removal() {
    nad_HMap *m = make_filled(nad_hash_i32, 3);

    TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, 1));
    put(m, 1, 111);

    TEST_ASSERT_EQUAL_size_t(3, nad_hmap_len(m));
    assert_has(m, 1, 111);

    nad_hmap_drop(m);
}

static void test_clear_empties_and_keeps_the_buckets() {
    nad_HMap *m = make_filled(nad_hash_i32, 20);
    const size_t buckets = nad_hmap_bucket_count(m);

    nad_hmap_clear(m);

    TEST_ASSERT_EQUAL_size_t(0, nad_hmap_len(m));
    TEST_ASSERT_EQUAL_size_t(buckets, nad_hmap_bucket_count(m));
    TEST_ASSERT_NULL(nad_hmap_first_node(m));
    assert_missing(m, 1);

    nad_hmap_drop(m);
}

static void test_clear_leaves_a_usable_map() {
    nad_HMap *m = make_filled(nad_hash_i32, 10);

    nad_hmap_clear(m);
    put(m, 5, 55);

    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(m));
    assert_has(m, 5, 55);

    nad_hmap_drop(m);
}

/* ========== walk ========== */

static void test_the_walk_reaches_every_entry_once() {
    nad_HMap *m = make_filled(nad_hash_i32, 50);

    assert_walk_sees_everything(m);

    nad_hmap_drop(m);
}

// one bucket holds everything, so the walk is one chain and never has to skip
static void test_the_walk_crosses_a_single_chain() {
    nad_HMap *m = make_filled(hash_all_alike, 12);

    assert_walk_sees_everything(m);

    nad_hmap_drop(m);
}

// two full buckets among many empty ones: the walk has to step over the gaps
static void test_the_walk_skips_the_empty_buckets() {
    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW_CAP(int32_t, int32_t, 64, hash_by_parity, nad_eq_i32, nad_al_default(), &m));
    for (int32_t i = 0; i < 10; ++i) {
        put(m, i, i * 10);
    }

    TEST_ASSERT_TRUE(nad_hmap_bucket_count(m) >= 64);
    assert_walk_sees_everything(m);

    nad_hmap_drop(m);
}

static void test_the_walk_of_an_empty_map_stops_at_once() {
    nad_HMap *m = make_map(nad_hash_i32);
    TEST_ASSERT_NULL(nad_hmap_first_node(m));

    nad_HMap *cleared = make_filled(nad_hash_i32, 5);
    nad_hmap_clear(cleared);
    TEST_ASSERT_NULL(nad_hmap_first_node(cleared));

    nad_hmap_drop(m);
    nad_hmap_drop(cleared);
}

static void test_the_walk_after_a_removal_sees_the_rest() {
    nad_HMap *m = make_filled(nad_hash_i32, 30);

    for (int32_t i = 0; i < 30; i += 2) {
        TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, i));
    }

    TEST_ASSERT_EQUAL_size_t(15, nad_hmap_len(m));
    assert_walk_sees_everything(m);

    nad_hmap_drop(m);
}

/* ========== get_or_insert ========== */

static void test_get_or_insert_puts_a_missing_key_in() {
    nad_HMap *m = make_map(nad_hash_i32);

    nad_HMapNode *node = nullptr;
    NAD_TEST_OK(NAD_HMAP_GET_OR_INSERT(int32_t, int32_t, m, 1, 10, &node));

    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT32(1, *NAD_HMAP_NODE_KEY_AS(int32_t, node));
    TEST_ASSERT_EQUAL_INT32(10, *NAD_HMAP_NODE_VAL_AS(int32_t, m, node));
    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(m));
    assert_has(m, 1, 10);

    nad_hmap_drop(m);
}

// unlike insert, it does NOT overwrite: the value it carries is only for a key that was
// not there
static void test_get_or_insert_leaves_a_present_key_alone() {
    nad_HMap *m = make_map(nad_hash_i32);
    put(m, 1, 10);

    nad_HMapNode *node = nullptr;
    NAD_TEST_OK(NAD_HMAP_GET_OR_INSERT(int32_t, int32_t, m, 1, 999, &node));

    TEST_ASSERT_EQUAL_INT32(10, *NAD_HMAP_NODE_VAL_AS(int32_t, m, node));
    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(m));

    nad_hmap_drop(m);
}

static void test_get_or_insert_hands_back_the_node_that_is_already_there() {
    nad_HMap *m = make_filled(hash_all_alike, 5);
    const nad_HMapNode *held = NAD_HMAP_FIND(int32_t, m, 3);

    nad_HMapNode *node = nullptr;
    NAD_TEST_OK(NAD_HMAP_GET_OR_INSERT(int32_t, int32_t, m, 3, 0, &node));

    TEST_ASSERT_EQUAL_PTR(held, node);

    nad_hmap_drop(m);
}

// the whole reason the operation exists. Reaching the same result through get_mut and
// then insert asks the hasher twice whenever the key turns out to be absent
static void test_get_or_insert_hashes_the_key_once() {
    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, hash_i32_counting, nad_eq_i32, nad_al_default(), &m));

    hash_calls = 0;
    nad_HMapNode *node = nullptr;
    NAD_TEST_OK(NAD_HMAP_GET_OR_INSERT(int32_t, int32_t, m, 1, 10, &node));
    TEST_ASSERT_EQUAL_size_t(1, hash_calls);

    // the pair it replaces, on a key that is not there either
    hash_calls = 0;
    if (!nad_hmap_get_mut(m, &(int32_t){ 2 })) {
        NAD_TEST_OK(NAD_HMAP_INSERT(int32_t, int32_t, m, 2, 20, nullptr));
    }
    TEST_ASSERT_EQUAL_size_t(2, hash_calls);

    // and once again on a key that IS there, where both forms cost the same
    hash_calls = 0;
    NAD_TEST_OK(NAD_HMAP_GET_OR_INSERT(int32_t, int32_t, m, 1, 0, &node));
    TEST_ASSERT_EQUAL_size_t(1, hash_calls);

    nad_hmap_drop(m);
}

// the idiom it was added for: a tally that never looks a key up twice
static void test_get_or_insert_carries_the_counter_idiom() {
    nad_HMap *m = make_map(nad_hash_i32);

    constexpr int32_t seen[6] = {3, 1, 3, 3, 1, 7};
    for (size_t i = 0; i < 6; ++i) {
        nad_HMapNode *node = nullptr;
        NAD_TEST_OK(nad_hmap_get_or_insert(m, &seen[i], &(int32_t){ 0 }, &node));
        ++*NAD_HMAP_NODE_VAL_MUT_AS(int32_t, m, node);
    }

    TEST_ASSERT_EQUAL_size_t(3, nad_hmap_len(m));
    assert_has(m, 3, 3);
    assert_has(m, 1, 2);
    assert_has(m, 7, 1);

    nad_hmap_drop(m);
}

/* ========== shrink_to_fit ========== */

static void test_shrink_to_fit_gives_the_buckets_back() {
    nad_HMap *m = make_filled(nad_hash_i32, 300);
    const size_t grown = nad_hmap_bucket_count(m);

    for (int32_t i = 3; i < 300; ++i) {
        TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, i));
    }

    NAD_TEST_OK(nad_hmap_shrink_to_fit(m));

    TEST_ASSERT_TRUE(nad_hmap_bucket_count(m) < grown);
    TEST_ASSERT_TRUE(nad_hmap_bucket_count(m) >= nad_hmap_len(m));
    for (int32_t i = 0; i < 3; ++i) {
        assert_has(m, i, i * 10);
    }
    assert_walk_sees_everything(m);

    nad_hmap_drop(m);
}

// the floor is the count a fresh map takes: a nearly empty map lands exactly where a new
// one starts, not at some denser shape that happens to still work
static void test_shrink_to_fit_stops_at_the_starting_size() {
    nad_HMap *fresh = make_map(nad_hash_i32);
    put(fresh, 0, 0);
    const size_t base = nad_hmap_bucket_count(fresh);

    nad_HMap *m = make_filled(nad_hash_i32, 300);
    for (int32_t i = 1; i < 300; ++i) {
        TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, i));
    }

    NAD_TEST_OK(nad_hmap_shrink_to_fit(m));

    TEST_ASSERT_EQUAL_size_t(base, nad_hmap_bucket_count(m));
    assert_has(m, 0, 0);

    nad_hmap_drop(fresh);
    nad_hmap_drop(m);
}

// nothing left to hold, so the map owns nothing at all — as it did when it was made
static void test_shrink_to_fit_of_an_empty_map_owns_nothing() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, &al, &m));
    for (int32_t i = 0; i < 50; ++i) {
        put(m, i, i);
    }
    nad_hmap_clear(m);

    NAD_TEST_OK(nad_hmap_shrink_to_fit(m));

    TEST_ASSERT_EQUAL_size_t(0, nad_hmap_bucket_count(m));
    TEST_ASSERT_EQUAL_size_t(1, probe.live); // the header alone
    TEST_ASSERT_NULL(nad_hmap_first_node(m));

    // and it is still a map: the next insert takes buckets again
    put(m, 1, 10);
    assert_has(m, 1, 10);

    nad_hmap_drop(m);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// shrinking relinks the entries where they lie, exactly as growing does, so a borrowed
// node comes through it
static void test_shrink_to_fit_keeps_borrowed_nodes() {
    nad_HMap *m = make_filled(nad_hash_i32, 200);

    const nad_HMapNode *held[3];
    for (int32_t i = 0; i < 3; ++i) {
        held[i] = NAD_HMAP_FIND(int32_t, m, i);
    }

    for (int32_t i = 3; i < 200; ++i) {
        TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, i));
    }
    NAD_TEST_OK(nad_hmap_shrink_to_fit(m));

    for (int32_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_PTR(held[i], NAD_HMAP_FIND(int32_t, m, i));
        TEST_ASSERT_EQUAL_INT32(i * 10, *NAD_HMAP_NODE_VAL_AS(int32_t, m, held[i]));
    }

    nad_hmap_drop(m);
}

static void test_shrink_to_fit_allocates_only_the_bucket_array() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, &al, &m));
    for (int32_t i = 0; i < 100; ++i) {
        put(m, i, i);
    }
    for (int32_t i = 2; i < 100; ++i) {
        TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, i));
    }

    const size_t before = nad_test_probe_requests(&probe);
    NAD_TEST_OK(nad_hmap_shrink_to_fit(m));

    TEST_ASSERT_EQUAL_size_t(1, nad_test_probe_requests(&probe) - before);

    nad_hmap_drop(m);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// a map that is already as small as it can be asks the allocator for nothing
static void test_shrink_to_fit_when_already_tight_is_a_noop() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, &al, &m));
    put(m, 1, 10);

    NAD_TEST_OK(nad_hmap_shrink_to_fit(m));
    const size_t buckets = nad_hmap_bucket_count(m);
    const size_t before = nad_test_probe_requests(&probe);

    NAD_TEST_OK(nad_hmap_shrink_to_fit(m));

    TEST_ASSERT_EQUAL_size_t(before, nad_test_probe_requests(&probe));
    TEST_ASSERT_EQUAL_size_t(buckets, nad_hmap_bucket_count(m));

    nad_hmap_drop(m);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_a_shrunk_map_grows_again() {
    nad_HMap *m = make_filled(nad_hash_i32, 100);
    for (int32_t i = 2; i < 100; ++i) {
        TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, m, i));
    }
    NAD_TEST_OK(nad_hmap_shrink_to_fit(m));

    for (int32_t i = 2; i < 100; ++i) {
        put(m, i, i * 10);
    }

    TEST_ASSERT_EQUAL_size_t(100, nad_hmap_len(m));
    for (int32_t i = 0; i < 100; ++i) {
        assert_has(m, i, i * 10);
    }

    nad_hmap_drop(m);
}

/* ========== copy ========== */

static void test_copy_is_independent() {
    nad_HMap *src = make_filled(nad_hash_i32, 10);

    nad_HMap *dst = nullptr;
    NAD_TEST_OK(nad_hmap_copy(src, &dst));

    put(dst, 0, 999);
    TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, dst, 1));

    assert_has(src, 0, 0);
    assert_has(src, 1, 10);
    assert_has(dst, 0, 999);
    assert_missing(dst, 1);

    nad_hmap_drop(src);
    nad_hmap_drop(dst);
}

static void test_copy_carries_the_hasher_and_the_equality() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *src = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, hash_all_alike, nad_eq_i32, arena, &src));
    put(src, 1, 10);

    nad_HMap *dst = nullptr;
    NAD_TEST_OK(nad_hmap_copy(src, &dst));

    TEST_ASSERT_EQUAL_PTR(hash_all_alike, nad_hmap_hasher(dst));
    TEST_ASSERT_EQUAL_PTR(nad_eq_i32, nad_hmap_key_eq(dst));
    TEST_ASSERT_EQUAL_PTR(arena, nad_hmap_al(dst));
    assert_has(dst, 1, 10);

    nad_hmap_drop(src);
    nad_hmap_drop(dst);
    nad_al_arena_drop(arena);
}

static void test_copy_of_empty_stays_empty() {
    nad_HMap *src = make_map(nad_hash_i32);

    nad_HMap *dst = nullptr;
    NAD_TEST_OK(nad_hmap_copy(src, &dst));

    TEST_ASSERT_EQUAL_size_t(0, nad_hmap_len(dst));

    nad_hmap_drop(src);
    nad_hmap_drop(dst);
}

static void test_copy_assign_overwrites_the_target() {
    nad_HMap *src = make_filled(nad_hash_i32, 6);
    nad_HMap *dst = make_filled(nad_hash_i32, 2);
    put(dst, 100, 100);

    NAD_TEST_OK(nad_hmap_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(6, nad_hmap_len(dst));
    assert_missing(dst, 100);
    for (int32_t i = 0; i < 6; ++i) {
        assert_has(dst, i, i * 10);
    }

    nad_hmap_drop(src);
    nad_hmap_drop(dst);
}

static void test_copy_assign_hands_over_the_hasher_too() {
    nad_HMap *src = make_map(hash_all_alike);
    put(src, 1, 10);

    nad_HMap *dst = make_map(nad_hash_i32);
    put(dst, 2, 20);

    NAD_TEST_OK(nad_hmap_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_PTR(hash_all_alike, nad_hmap_hasher(dst));
    assert_has(dst, 1, 10);
    assert_missing(dst, 2);

    nad_hmap_drop(src);
    nad_hmap_drop(dst);
}

static void test_copy_assign_self_is_noop() {
    nad_HMap *m = make_filled(nad_hash_i32, 5);

    NAD_TEST_OK(nad_hmap_copy_assign(m, m));

    TEST_ASSERT_EQUAL_size_t(5, nad_hmap_len(m));
    assert_has(m, 3, 30);

    nad_hmap_drop(m);
}

static void test_copy_assign_keeps_the_target_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *src = make_filled(nad_hash_i32, 4);

    nad_HMap *dst = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &dst));

    NAD_TEST_OK(nad_hmap_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_PTR(arena, nad_hmap_al(dst));
    TEST_ASSERT_EQUAL_size_t(4, nad_hmap_len(dst));

    nad_hmap_drop(src);
    nad_hmap_drop(dst);
    nad_al_arena_drop(arena);
}

// what the target held before must go back to the allocator rather than be stranded when
// the clone takes its place. The default allocator would say nothing about it
static void test_copy_assign_hands_back_the_old_contents() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HMap *dst = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, &al, &dst));
    for (int32_t i = 0; i < 20; ++i) {
        put(dst, i, i);
    }

    nad_HMap *src = make_filled(nad_hash_i32, 3);
    NAD_TEST_OK(nad_hmap_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(3, nad_hmap_len(dst));

    nad_hmap_drop(src);
    nad_hmap_drop(dst);

    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

/* ========== swap ========== */

static void test_swap_exchanges_the_entries_and_the_hashers() {
    nad_HMap *a = make_map(nad_hash_i32);
    put(a, 1, 10);

    nad_HMap *b = make_map(hash_all_alike);
    put(b, 2, 20);
    put(b, 3, 30);

    nad_hmap_swap(a, b);

    TEST_ASSERT_EQUAL_size_t(2, nad_hmap_len(a));
    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(b));
    TEST_ASSERT_EQUAL_PTR(hash_all_alike, nad_hmap_hasher(a));
    TEST_ASSERT_EQUAL_PTR(nad_hash_i32, nad_hmap_hasher(b));
    assert_has(a, 2, 20);
    assert_has(b, 1, 10);

    nad_hmap_drop(a);
    nad_hmap_drop(b);
}

// the nodes change map without moving, which is why swap wants one allocator
static void test_swap_keeps_the_nodes_alive() {
    nad_HMap *a = make_filled(nad_hash_i32, 4);
    nad_HMap *b = make_map(nad_hash_i32);

    const nad_HMapNode *held = NAD_HMAP_FIND(int32_t, a, 2);
    TEST_ASSERT_NOT_NULL(held);

    nad_hmap_swap(a, b);

    TEST_ASSERT_EQUAL_PTR(held, NAD_HMAP_FIND(int32_t, b, 2));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
}

static void test_swap_self_is_noop() {
    nad_HMap *m = make_filled(nad_hash_i32, 3);

    nad_hmap_swap(m, m);

    TEST_ASSERT_EQUAL_size_t(3, nad_hmap_len(m));
    assert_has(m, 1, 10);

    nad_hmap_drop(m);
}

/* ========== allocation failure ========== */

static void test_new_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(arena);
    nad_test_arena_leave(arena, 0);

    nad_HMap *m = nullptr;
    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &m)
    );
    TEST_ASSERT_NULL(m);

    nad_al_arena_drop(arena);
}

// the header is taken and the bucket array is refused: the header must not be stranded
static void test_new_cap_frees_the_header_when_the_buckets_are_refused() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_test_probe_fail_after_next(&probe, 1);

    nad_HMap *m = nullptr;
    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_HMAP_NEW_CAP(int32_t, int32_t, 16, nad_hash_i32, nad_eq_i32, &al, &m)
    );

    TEST_ASSERT_NULL(m);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_insert_reports_an_exhausted_arena_and_changes_nothing() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &m));
    put(m, 1, 10);
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, NAD_HMAP_INSERT(int32_t, int32_t, m, 2, 20, nullptr));

    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(m));
    assert_has(m, 1, 10);
    assert_missing(m, 2);

    nad_al_arena_drop(arena);
}

// overwriting an existing key needs no memory at all, so an exhausted arena is no
// obstacle to it
static void test_an_overwrite_needs_no_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &m));
    put(m, 1, 10);
    nad_test_arena_leave(arena, 0);

    NAD_TEST_OK(NAD_HMAP_INSERT(int32_t, int32_t, m, 1, 99, nullptr));
    assert_has(m, 1, 99);

    nad_al_arena_drop(arena);
}

static void test_reserve_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &m));
    put(m, 1, 10);
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_hmap_reserve(m, 100000));

    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(m));
    assert_has(m, 1, 10);

    nad_al_arena_drop(arena);
}

static void test_copy_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *src = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &src));
    put(src, 1, 10);
    nad_test_arena_leave(arena, 0);

    nad_HMap *dst = nullptr;
    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_hmap_copy(src, &dst));
    TEST_ASSERT_NULL(dst);

    nad_al_arena_drop(arena);
}

// a refused clone must leave the target whole, not half overwritten
static void test_copy_assign_leaves_the_target_untouched_on_failure() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *src = make_filled(nad_hash_i32, 50);

    nad_HMap *dst = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &dst));
    put(dst, 7, 70);
    nad_test_arena_leave(arena, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, nad_hmap_copy_assign(src, dst));

    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(dst));
    assert_has(dst, 7, 70);

    nad_hmap_drop(src);
    nad_al_arena_drop(arena);
}

// the node is taken and then the map is left as it was: nothing leaks on the way out
static void test_a_refused_node_leaves_nothing_behind() {
    nad_TestProbe probe;
    nad_test_probe_reset(&probe);
    nad_Al al = nad_test_probe_full(&probe);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, &al, &m));
    put(m, 1, 10);

    nad_test_probe_fail_after_next(&probe, 0);

    NAD_TEST_STATUS(NAD_STATUS_OUT_OF_MEMORY, NAD_HMAP_INSERT(int32_t, int32_t, m, 2, 20, nullptr));

    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(m));
    assert_has(m, 1, 10);

    nad_test_probe_reset(&probe);
    nad_hmap_drop(m);
}

static void test_get_or_insert_reports_an_exhausted_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &m));
    put(m, 1, 10);
    nad_test_arena_leave(arena, 0);

    nad_HMapNode *node = nullptr;
    NAD_TEST_STATUS(
        NAD_STATUS_OUT_OF_MEMORY,
        NAD_HMAP_GET_OR_INSERT(int32_t, int32_t, m, 2, 20, &node)
    );

    TEST_ASSERT_EQUAL_size_t(1, nad_hmap_len(m));
    assert_missing(m, 2);

    nad_al_arena_drop(arena);
}

// finding a key that is there needs no memory, so an exhausted arena is no obstacle
static void test_get_or_insert_of_a_present_key_needs_no_allocator() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 512);
    TEST_ASSERT_NOT_NULL(arena);

    nad_HMap *m = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, int32_t, nad_hash_i32, nad_eq_i32, arena, &m));
    put(m, 1, 10);
    nad_test_arena_leave(arena, 0);

    nad_HMapNode *node = nullptr;
    NAD_TEST_OK(NAD_HMAP_GET_OR_INSERT(int32_t, int32_t, m, 1, 999, &node));
    TEST_ASSERT_EQUAL_INT32(10, *NAD_HMAP_NODE_VAL_AS(int32_t, m, node));

    nad_al_arena_drop(arena);
}

/* ========== compare ========== */

// an equality over the value side that sees less than the bytes do, to show that what
// counts as an equal value is the caller's call and not the map's
static bool eq_i32_abs(const void *lhs, const void *rhs) {
    const int32_t a = *(const int32_t *) lhs;
    const int32_t b = *(const int32_t *) rhs;

    return (a < 0 ? -a : a) == (b < 0 ? -b : b);
}

// neither the order the entries went in nor the number of buckets they landed in is part
// of what a map holds
static void test_eq_ignores_insertion_order_and_bucket_count() {
    nad_HMap *a = make_filled(nad_hash_i32, 40);

    nad_HMap *b = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW_CAP(int32_t, int32_t, 256, nad_hash_i32, nad_eq_i32, nad_al_default(), &b));
    for (int32_t i = 39; i >= 0; --i) {
        put(b, i, i * 10);
    }

    TEST_ASSERT_TRUE(nad_hmap_bucket_count(a) != nad_hmap_bucket_count(b));
    TEST_ASSERT_TRUE(nad_hmap_eq(a, a));
    TEST_ASSERT_TRUE(nad_hmap_eq(a, b));
    TEST_ASSERT_TRUE(nad_hmap_eq(b, a));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
}

// the keys of 'a' are looked up in 'b', so it is the hasher of 'b' that has to answer:
// a hash taken from 'a' would point at the wrong bucket of a differently hashed table
static void test_eq_looks_the_keys_up_with_the_hasher_of_the_other() {
    nad_HMap *a = make_filled(nad_hash_i32, 12);
    nad_HMap *b = make_filled(hash_all_alike, 12);

    TEST_ASSERT_TRUE(nad_hmap_eq(a, b));
    TEST_ASSERT_TRUE(nad_hmap_eq(b, a));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
}

static void test_eq_parts_a_differing_value() {
    nad_HMap *a = make_filled(nad_hash_i32, 8);
    nad_HMap *b = make_filled(nad_hash_i32, 8);
    put(b, 3, -30);

    TEST_ASSERT_EQUAL_size_t(nad_hmap_len(a), nad_hmap_len(b));
    TEST_ASSERT_FALSE(nad_hmap_eq(a, b));
    TEST_ASSERT_FALSE(nad_hmap_eq(b, a));

    // ... unless the equality the caller names forgives the difference
    TEST_ASSERT_TRUE(nad_hmap_eq_by(a, b, eq_i32_abs));
    TEST_ASSERT_TRUE(nad_hmap_eq_by(b, a, eq_i32_abs));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
}

// the same number of entries, one key in place of another
static void test_eq_parts_a_differing_key() {
    nad_HMap *a = make_filled(nad_hash_i32, 8);
    nad_HMap *b = make_filled(nad_hash_i32, 8);
    TEST_ASSERT_TRUE(NAD_HMAP_REMOVE(int32_t, b, 3));
    put(b, 100, 30);

    TEST_ASSERT_EQUAL_size_t(nad_hmap_len(a), nad_hmap_len(b));
    TEST_ASSERT_FALSE(nad_hmap_eq(a, b));
    TEST_ASSERT_FALSE(nad_hmap_eq(b, a));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
}

// one is a proper subset of the other, so only the length says no
static void test_eq_parts_different_lengths() {
    nad_HMap *a = make_filled(nad_hash_i32, 8);
    nad_HMap *smaller = make_filled(nad_hash_i32, 7);

    TEST_ASSERT_FALSE(nad_hmap_eq(a, smaller));
    TEST_ASSERT_FALSE(nad_hmap_eq(smaller, a));

    nad_hmap_drop(a);
    nad_hmap_drop(smaller);
}

static void test_eq_of_two_empties() {
    nad_HMap *a = make_map(nad_hash_i32);
    nad_HMap *b = make_filled(nad_hash_i32, 8);
    nad_HMap *one = make_filled(nad_hash_i32, 1);

    nad_hmap_clear(b);

    TEST_ASSERT_TRUE(nad_hmap_eq(a, b));
    TEST_ASSERT_TRUE(nad_hmap_eq(b, a));
    TEST_ASSERT_FALSE(nad_hmap_eq(a, one));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
    nad_hmap_drop(one);
}

static void test_eq_walks_whole_chains() {
    nad_HMap *a = make_filled(hash_all_alike, 16);
    nad_HMap *b = make_filled(hash_all_alike, 16);
    put(b, 15, -150);

    TEST_ASSERT_TRUE(nad_hmap_eq(a, a));
    TEST_ASSERT_FALSE(nad_hmap_eq(a, b));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
}

// a value with a field that does not count is what the second door is for: these Pairs
// agree in the first field and differ in the second
static void test_eq_by_asks_the_equality_for_the_value_side() {
    nad_HMap *a = nullptr;
    nad_HMap *b = nullptr;
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, Pair, nad_hash_i32, nad_eq_i32, nad_al_default(), &a));
    NAD_TEST_OK(NAD_HMAP_NEW(int32_t, Pair, nad_hash_i32, nad_eq_i32, nad_al_default(), &b));

    for (int32_t i = 0; i < 8; ++i) {
        NAD_TEST_OK(nad_hmap_insert(a, &i, &(Pair){i, 10}, nullptr));
        NAD_TEST_OK(nad_hmap_insert(b, &i, &(Pair){i, 70}, nullptr));
    }

    TEST_ASSERT_FALSE(nad_hmap_eq(a, b));
    TEST_ASSERT_TRUE(nad_hmap_eq_by(a, b, nad_test_pair_eq_a));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
}

// the two doors are one walk: wherever the equality IS the bytes they answer alike
static void test_eq_and_eq_by_agree_on_plain_values() {
    nad_HMap *a = make_filled(nad_hash_i32, 16);
    nad_HMap *b = make_filled(nad_hash_i32, 16);
    nad_HMap *smaller = make_filled(nad_hash_i32, 15);

    TEST_ASSERT_EQUAL(nad_hmap_eq(a, b), nad_hmap_eq_by(a, b, nad_eq_i32));
    TEST_ASSERT_EQUAL(nad_hmap_eq(a, smaller), nad_hmap_eq_by(a, smaller, nad_eq_i32));

    put(b, 7, -70);
    TEST_ASSERT_EQUAL(nad_hmap_eq(a, b), nad_hmap_eq_by(a, b, nad_eq_i32));

    nad_hmap_drop(a);
    nad_hmap_drop(b);
    nad_hmap_drop(smaller);
}

static void test_eq_matches_a_copy() {
    nad_HMap *a = make_filled(nad_hash_i32, 24);

    nad_HMap *copy = nullptr;
    NAD_TEST_OK(nad_hmap_copy(a, &copy));

    TEST_ASSERT_TRUE(nad_hmap_eq(a, copy));

    nad_hmap_drop(a);
    nad_hmap_drop(copy);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_empty);
    RUN_TEST(test_new_takes_no_buckets_until_the_first_insert);
    RUN_TEST(test_new_cap_reserves_buckets_without_entries);
    RUN_TEST(test_drop_null_is_noop);
    RUN_TEST(test_drop_hands_back_every_node);

    RUN_TEST(test_insert_then_get);
    RUN_TEST(test_get_of_a_missing_key_is_null);
    RUN_TEST(test_get_on_an_empty_map_is_null);
    RUN_TEST(test_insert_overwrites_an_existing_key);
    RUN_TEST(test_insert_reports_whether_the_key_was_new);
    RUN_TEST(test_get_mut_writes_through);
    RUN_TEST(test_find_gives_the_entry_as_a_node);
    RUN_TEST(test_node_val_mut_writes_through);
    RUN_TEST(test_a_map_whose_keys_all_collide_still_finds_them);
    RUN_TEST(test_colliding_keys_overwrite_only_their_own_entry);
    RUN_TEST(test_a_hash_collision_is_not_an_equality);
    RUN_TEST(test_wide_keys_and_values_travel_whole);
    RUN_TEST(test_the_chain_is_walked_by_hash_before_eq);

    RUN_TEST(test_the_buckets_double_as_the_map_fills);
    RUN_TEST(test_every_entry_survives_the_growths);
    RUN_TEST(test_a_borrowed_node_survives_every_growth);
    RUN_TEST(test_a_growth_allocates_only_the_bucket_array);
    RUN_TEST(test_reserve_grows_the_buckets_only);
    RUN_TEST(test_reserve_below_the_bucket_count_changes_nothing);

    RUN_TEST(test_get_or_insert_puts_a_missing_key_in);
    RUN_TEST(test_get_or_insert_leaves_a_present_key_alone);
    RUN_TEST(test_get_or_insert_hands_back_the_node_that_is_already_there);
    RUN_TEST(test_get_or_insert_hashes_the_key_once);
    RUN_TEST(test_get_or_insert_carries_the_counter_idiom);

    RUN_TEST(test_shrink_to_fit_gives_the_buckets_back);
    RUN_TEST(test_shrink_to_fit_stops_at_the_starting_size);
    RUN_TEST(test_shrink_to_fit_of_an_empty_map_owns_nothing);
    RUN_TEST(test_shrink_to_fit_keeps_borrowed_nodes);
    RUN_TEST(test_shrink_to_fit_allocates_only_the_bucket_array);
    RUN_TEST(test_shrink_to_fit_when_already_tight_is_a_noop);
    RUN_TEST(test_a_shrunk_map_grows_again);

    RUN_TEST(test_remove_takes_the_entry_out);
    RUN_TEST(test_remove_of_a_missing_key_says_so);
    RUN_TEST(test_remove_on_an_empty_map_says_so);
    RUN_TEST(test_remove_from_the_middle_of_a_chain);
    RUN_TEST(test_remove_every_key_of_a_chain_in_turn);
    RUN_TEST(test_remove_node_drops_the_entry_it_names);
    RUN_TEST(test_a_key_can_be_put_back_after_removal);
    RUN_TEST(test_clear_empties_and_keeps_the_buckets);
    RUN_TEST(test_clear_leaves_a_usable_map);

    RUN_TEST(test_the_walk_reaches_every_entry_once);
    RUN_TEST(test_the_walk_crosses_a_single_chain);
    RUN_TEST(test_the_walk_skips_the_empty_buckets);
    RUN_TEST(test_the_walk_of_an_empty_map_stops_at_once);
    RUN_TEST(test_the_walk_after_a_removal_sees_the_rest);

    RUN_TEST(test_copy_is_independent);
    RUN_TEST(test_copy_carries_the_hasher_and_the_equality);
    RUN_TEST(test_copy_of_empty_stays_empty);
    RUN_TEST(test_copy_assign_overwrites_the_target);
    RUN_TEST(test_copy_assign_hands_over_the_hasher_too);
    RUN_TEST(test_copy_assign_self_is_noop);
    RUN_TEST(test_copy_assign_keeps_the_target_allocator);
    RUN_TEST(test_copy_assign_hands_back_the_old_contents);

    RUN_TEST(test_swap_exchanges_the_entries_and_the_hashers);
    RUN_TEST(test_swap_keeps_the_nodes_alive);
    RUN_TEST(test_swap_self_is_noop);

    RUN_TEST(test_new_reports_an_exhausted_arena);
    RUN_TEST(test_new_cap_frees_the_header_when_the_buckets_are_refused);
    RUN_TEST(test_insert_reports_an_exhausted_arena_and_changes_nothing);
    RUN_TEST(test_an_overwrite_needs_no_allocator);
    RUN_TEST(test_reserve_reports_an_exhausted_arena);
    RUN_TEST(test_copy_reports_an_exhausted_arena);
    RUN_TEST(test_copy_assign_leaves_the_target_untouched_on_failure);
    RUN_TEST(test_a_refused_node_leaves_nothing_behind);
    RUN_TEST(test_get_or_insert_reports_an_exhausted_arena);
    RUN_TEST(test_get_or_insert_of_a_present_key_needs_no_allocator);


    RUN_TEST(test_eq_ignores_insertion_order_and_bucket_count);
    RUN_TEST(test_eq_looks_the_keys_up_with_the_hasher_of_the_other);
    RUN_TEST(test_eq_parts_a_differing_value);
    RUN_TEST(test_eq_parts_a_differing_key);
    RUN_TEST(test_eq_parts_different_lengths);
    RUN_TEST(test_eq_of_two_empties);
    RUN_TEST(test_eq_walks_whole_chains);
    RUN_TEST(test_eq_by_asks_the_equality_for_the_value_side);
    RUN_TEST(test_eq_and_eq_by_agree_on_plain_values);
    RUN_TEST(test_eq_matches_a_copy);

    return UNITY_END();
}
