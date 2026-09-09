#include "tda/alloc/pool.h"
#include "tda/alloc/default.h"

#include <unity.h>

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// blocks are rounded up to this, and must also hold a free-list pointer
static constexpr size_t ALIGNMENT = alignof(max_align_t);

static size_t aligned(size_t size) {
    if (size < sizeof(void *)) {
        size = sizeof(void *);
    }
    return (size + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
}

/* ========== probe allocator ==========
 *
 * Counts live blocks and can fail on demand, so the pool's construction and
 * teardown paths can be checked for leaks.
 */

typedef struct {
    size_t live;
    size_t alloc_calls;
    size_t fail_after;
} Probe;

static Probe probe;

static void *probe_alloc(void *ctx, size_t size) {
    Probe *p = ctx;
    ++p->alloc_calls;

    if (p->alloc_calls > p->fail_after) {
        return nullptr;
    }

    void *ptr = malloc(size);
    if (ptr) {
        ++p->live;
    }
    return ptr;
}

static void probe_dealloc(void *ctx, void *ptr, size_t size) {
    Probe *p = ctx;
    (void) size;

    --p->live;
    free(ptr);
}

static tda_Al probe_al() {
    return (tda_Al){
        .ctx = &probe,
        .alloc = probe_alloc,
        .calloc = nullptr,
        .realloc = nullptr,
        .dealloc = probe_dealloc,
    };
}

void setUp() {
    probe = (Probe){.fail_after = SIZE_MAX};
}

void tearDown() {
}

/* ========== lifetime ========== */

static void test_new_starts_with_every_block_free() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 4);
    TEST_ASSERT_NOT_NULL(pool);

    const tda_AlPoolStats st = tda_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(32, st.block_size);
    TEST_ASSERT_EQUAL_size_t(4, st.block_count);
    TEST_ASSERT_EQUAL_size_t(0, st.used);
    TEST_ASSERT_EQUAL_size_t(4, st.free);

    tda_al_pool_drop(pool);
}

// a block too small to hold the free-list pointer is grown, then aligned up
static void test_new_raises_a_tiny_block_size() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 1, 4);
    TEST_ASSERT_NOT_NULL(pool);

    const tda_AlPoolStats st = tda_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(aligned(1), st.block_size);
    TEST_ASSERT_TRUE(st.block_size >= sizeof(void *));

    tda_al_pool_drop(pool);
}

static void test_new_rounds_the_block_size_up() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), ALIGNMENT + 1, 2);
    TEST_ASSERT_NOT_NULL(pool);

    TEST_ASSERT_EQUAL_size_t(2 * ALIGNMENT, tda_al_pool_stats(pool).block_size);

    tda_al_pool_drop(pool);
}

// the pool borrows its parent: everything it took must go back on drop
static void test_drop_returns_everything_to_the_parent() {
    tda_Al parent = probe_al();

    tda_Al *pool = tda_al_pool_new(&parent, 32, 4);
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_EQUAL_size_t(3, probe.live); // context, buffer, the tda_Al itself

    tda_al_pool_drop(pool);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// each construction step can fail; none of them may leak the earlier ones
static void test_new_cleans_up_after_a_failing_parent() {
    for (size_t fail_at = 0; fail_at < 3; ++fail_at) {
        probe = (Probe){.fail_after = fail_at};
        tda_Al parent = probe_al();

        TEST_ASSERT_NULL(tda_al_pool_new(&parent, 32, 4));
        TEST_ASSERT_EQUAL_size_t(0, probe.live);
    }
}

static void test_drop_null_is_noop() {
    tda_al_pool_drop(nullptr);
}

/* ========== alloc ========== */

static void test_alloc_takes_one_block() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 4);

    TEST_ASSERT_NOT_NULL(tda_alloc(pool, 32));

    const tda_AlPoolStats st = tda_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(1, st.used);
    TEST_ASSERT_EQUAL_size_t(3, st.free);

    tda_al_pool_drop(pool);
}

// a request smaller than a block still consumes a whole block
static void test_alloc_of_a_partial_block_still_costs_one() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 64, 2);

    TEST_ASSERT_NOT_NULL(tda_alloc(pool, 1));
    TEST_ASSERT_EQUAL_size_t(1, tda_al_pool_stats(pool).used);

    tda_al_pool_drop(pool);
}

// the pool hands out fixed slots — anything larger cannot be served
static void test_alloc_larger_than_a_block_fails() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 4);

    TEST_ASSERT_NULL(tda_alloc(pool, 33));
    TEST_ASSERT_EQUAL_size_t(0, tda_al_pool_stats(pool).used);

    tda_al_pool_drop(pool);
}

static void test_alloc_zero_is_null_and_costs_nothing() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 4);

    TEST_ASSERT_NULL(tda_alloc(pool, 0));
    TEST_ASSERT_EQUAL_size_t(0, tda_al_pool_stats(pool).used);

    tda_al_pool_drop(pool);
}

static void test_exhaustion_returns_null() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 3);

    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_NOT_NULL(tda_alloc(pool, 32));
    }

    const tda_AlPoolStats st = tda_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(3, st.used);
    TEST_ASSERT_EQUAL_size_t(0, st.free);

    TEST_ASSERT_NULL(tda_alloc(pool, 32));
    TEST_ASSERT_EQUAL_size_t(3, tda_al_pool_stats(pool).used);

    tda_al_pool_drop(pool);
}

static void test_every_block_is_aligned() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 24, 4);

    for (size_t i = 0; i < 4; ++i) {
        void *p = tda_alloc(pool, 24);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) p % ALIGNMENT);
    }

    tda_al_pool_drop(pool);
}

// every block must be a distinct, non-overlapping region of the backing buffer
static void test_blocks_do_not_overlap() {
    constexpr size_t COUNT = 4;
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, COUNT);

    unsigned char *blocks[COUNT];
    for (size_t i = 0; i < COUNT; ++i) {
        blocks[i] = tda_alloc(pool, 32);
        TEST_ASSERT_NOT_NULL(blocks[i]);
        memset(blocks[i], (int) (i + 1), 32);
    }

    // if the slots overlapped, an earlier stamp would have been overwritten
    for (size_t i = 0; i < COUNT; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), blocks[i][0]);
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), blocks[i][31]);
    }

    tda_al_pool_drop(pool);
}

/* ========== dealloc ========== */

static void test_dealloc_returns_the_block() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 2);

    void *p = tda_alloc(pool, 32);
    TEST_ASSERT_EQUAL_size_t(1, tda_al_pool_stats(pool).used);

    tda_dealloc(pool, p, 32);

    const tda_AlPoolStats st = tda_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(0, st.used);
    TEST_ASSERT_EQUAL_size_t(2, st.free);

    tda_al_pool_drop(pool);
}

// the free list is LIFO: the block just returned is the next one handed out
static void test_freed_block_is_reused_first() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 3);

    void *a = tda_alloc(pool, 32);
    void *b = tda_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    tda_dealloc(pool, b, 32);
    TEST_ASSERT_EQUAL_PTR(b, tda_alloc(pool, 32));

    tda_al_pool_drop(pool);
}

// a fully drained pool becomes usable again once blocks come back
static void test_exhausted_pool_recovers_after_a_free() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 2);

    void *a = tda_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(tda_alloc(pool, 32));
    TEST_ASSERT_NULL(tda_alloc(pool, 32));

    tda_dealloc(pool, a, 32);
    TEST_ASSERT_EQUAL_PTR(a, tda_alloc(pool, 32));

    tda_al_pool_drop(pool);
}

static void test_dealloc_null_is_noop() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 2);

    TEST_ASSERT_NOT_NULL(tda_alloc(pool, 32));
    tda_dealloc(pool, nullptr, 32);
    TEST_ASSERT_EQUAL_size_t(1, tda_al_pool_stats(pool).used);

    tda_al_pool_drop(pool);
}

/* ========== calloc ========== */

static void test_calloc_zeroes_the_block() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 2);

    const unsigned char *p = tda_calloc(pool, 8, 4);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }

    tda_al_pool_drop(pool);
}

// a recycled block must not leak the previous tenant's bytes through calloc
static void test_calloc_zeroes_a_recycled_block() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 2);

    unsigned char *first = tda_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(first);
    memset(first, 0xFF, 32);
    tda_dealloc(pool, first, 32);

    const unsigned char *second = tda_calloc(pool, 8, 4);
    TEST_ASSERT_EQUAL_PTR(first, second);
    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, second[i]);
    }

    tda_al_pool_drop(pool);
}

static void test_calloc_larger_than_a_block_fails() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 2);

    TEST_ASSERT_NULL(tda_calloc(pool, 8, 8));
    TEST_ASSERT_EQUAL_size_t(0, tda_al_pool_stats(pool).used);

    tda_al_pool_drop(pool);
}

static void test_calloc_rejects_overflow() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 2);

    TEST_ASSERT_NULL(tda_calloc(pool, SIZE_MAX, 2));
    TEST_ASSERT_EQUAL_size_t(0, tda_al_pool_stats(pool).used);

    tda_al_pool_drop(pool);
}

/* ========== realloc ========== */

// the pool has no realloc hook; the fallback copies into a fresh block and
// returns the old one, so the net block count is unchanged
static void test_realloc_falls_back_to_a_fresh_block() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 3);

    unsigned char *p = tda_alloc(pool, 16);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 16; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    unsigned char *q = tda_realloc(pool, p, 16, 32);
    TEST_ASSERT_NOT_NULL(q);
    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), q[i]);
    }
    TEST_ASSERT_EQUAL_size_t(1, tda_al_pool_stats(pool).used);

    tda_al_pool_drop(pool);
}

/* ========== reset ========== */

static void test_reset_frees_every_block() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 3);

    void *first = tda_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(tda_alloc(pool, 32));
    TEST_ASSERT_NOT_NULL(tda_alloc(pool, 32));

    tda_al_pool_reset(pool);

    const tda_AlPoolStats st = tda_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(0, st.used);
    TEST_ASSERT_EQUAL_size_t(3, st.free);

    // the free list is rebuilt in order, so the first block comes back first
    TEST_ASSERT_EQUAL_PTR(first, tda_alloc(pool, 32));

    tda_al_pool_drop(pool);
}

// reset after partial frees must rebuild the list, not append to a stale one
static void test_reset_recovers_from_a_scrambled_free_list() {
    tda_Al *pool = tda_al_pool_new(tda_al_default(), 32, 3);

    void *a = tda_alloc(pool, 32);
    void *b = tda_alloc(pool, 32);
    tda_dealloc(pool, a, 32);
    tda_dealloc(pool, b, 32);

    tda_al_pool_reset(pool);
    TEST_ASSERT_EQUAL_size_t(3, tda_al_pool_stats(pool).free);

    // all three blocks must still be reachable, and all distinct
    void *got[3];
    for (size_t i = 0; i < 3; ++i) {
        got[i] = tda_alloc(pool, 32);
        TEST_ASSERT_NOT_NULL(got[i]);
    }
    TEST_ASSERT_TRUE(got[0] != got[1]);
    TEST_ASSERT_TRUE(got[1] != got[2]);
    TEST_ASSERT_TRUE(got[0] != got[2]);

    tda_al_pool_drop(pool);
}

/* ========== composition ========== */

// a pool is an ordinary allocator, so an arena can back it
static void test_pool_can_live_in_another_allocator() {
    tda_Al parent = probe_al();

    tda_Al *pool = tda_al_pool_new(&parent, 32, 2);
    TEST_ASSERT_NOT_NULL(pool);

    unsigned char *p = tda_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0x3C, 32);
    TEST_ASSERT_EQUAL_UINT8(0x3C, p[31]);

    tda_al_pool_drop(pool);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_with_every_block_free);
    RUN_TEST(test_new_raises_a_tiny_block_size);
    RUN_TEST(test_new_rounds_the_block_size_up);
    RUN_TEST(test_drop_returns_everything_to_the_parent);
    RUN_TEST(test_new_cleans_up_after_a_failing_parent);
    RUN_TEST(test_drop_null_is_noop);

    RUN_TEST(test_alloc_takes_one_block);
    RUN_TEST(test_alloc_of_a_partial_block_still_costs_one);
    RUN_TEST(test_alloc_larger_than_a_block_fails);
    RUN_TEST(test_alloc_zero_is_null_and_costs_nothing);
    RUN_TEST(test_exhaustion_returns_null);
    RUN_TEST(test_every_block_is_aligned);
    RUN_TEST(test_blocks_do_not_overlap);

    RUN_TEST(test_dealloc_returns_the_block);
    RUN_TEST(test_freed_block_is_reused_first);
    RUN_TEST(test_exhausted_pool_recovers_after_a_free);
    RUN_TEST(test_dealloc_null_is_noop);

    RUN_TEST(test_calloc_zeroes_the_block);
    RUN_TEST(test_calloc_zeroes_a_recycled_block);
    RUN_TEST(test_calloc_larger_than_a_block_fails);
    RUN_TEST(test_calloc_rejects_overflow);

    RUN_TEST(test_realloc_falls_back_to_a_fresh_block);

    RUN_TEST(test_reset_frees_every_block);
    RUN_TEST(test_reset_recovers_from_a_scrambled_free_list);

    RUN_TEST(test_pool_can_live_in_another_allocator);

    return UNITY_END();
}
