#include "nad/alloc/pool.h"
#include "nad/alloc/default.h"

#include "unity.h"

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

static nad_Al probe_al() {
    return (nad_Al){
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
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 4);
    TEST_ASSERT_NOT_NULL(pool);

    const nad_AlPoolStats st = nad_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(32, st.block_size);
    TEST_ASSERT_EQUAL_size_t(4, st.block_count);
    TEST_ASSERT_EQUAL_size_t(0, st.used);
    TEST_ASSERT_EQUAL_size_t(4, st.free);

    nad_al_pool_drop(pool);
}

// a block too small to hold the free-list pointer is grown, then aligned up
static void test_new_raises_a_tiny_block_size() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 1, 4);
    TEST_ASSERT_NOT_NULL(pool);

    const nad_AlPoolStats st = nad_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(aligned(1), st.block_size);
    TEST_ASSERT_TRUE(st.block_size >= sizeof(void *));

    nad_al_pool_drop(pool);
}

static void test_new_rounds_the_block_size_up() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), ALIGNMENT + 1, 2);
    TEST_ASSERT_NOT_NULL(pool);

    TEST_ASSERT_EQUAL_size_t(2 * ALIGNMENT, nad_al_pool_stats(pool).block_size);

    nad_al_pool_drop(pool);
}

// the pool borrows its parent: everything it took must go back on drop
static void test_drop_returns_everything_to_the_parent() {
    nad_Al parent = probe_al();

    nad_Al *pool = nad_al_pool_new(&parent, 32, 4);
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_EQUAL_size_t(3, probe.live); // context, buffer, the nad_Al itself

    nad_al_pool_drop(pool);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// each construction step can fail; none of them may leak the earlier ones
static void test_new_cleans_up_after_a_failing_parent() {
    for (size_t fail_at = 0; fail_at < 3; ++fail_at) {
        probe = (Probe){.fail_after = fail_at};
        nad_Al parent = probe_al();

        TEST_ASSERT_NULL(nad_al_pool_new(&parent, 32, 4));
        TEST_ASSERT_EQUAL_size_t(0, probe.live);
    }
}

static void test_drop_null_is_noop() {
    nad_al_pool_drop(nullptr);
}

/* ========== alloc ========== */

static void test_alloc_takes_one_block() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 4);

    TEST_ASSERT_NOT_NULL(nad_alloc(pool, 32));

    const nad_AlPoolStats st = nad_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(1, st.used);
    TEST_ASSERT_EQUAL_size_t(3, st.free);

    nad_al_pool_drop(pool);
}

// a request smaller than a block still consumes a whole block
static void test_alloc_of_a_partial_block_still_costs_one() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 64, 2);

    TEST_ASSERT_NOT_NULL(nad_alloc(pool, 1));
    TEST_ASSERT_EQUAL_size_t(1, nad_al_pool_stats(pool).used);

    nad_al_pool_drop(pool);
}

// the pool hands out fixed slots — anything larger cannot be served
static void test_alloc_larger_than_a_block_fails() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 4);

    TEST_ASSERT_NULL(nad_alloc(pool, 33));
    TEST_ASSERT_EQUAL_size_t(0, nad_al_pool_stats(pool).used);

    nad_al_pool_drop(pool);
}

static void test_alloc_zero_is_null_and_costs_nothing() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 4);

    TEST_ASSERT_NULL(nad_alloc(pool, 0));
    TEST_ASSERT_EQUAL_size_t(0, nad_al_pool_stats(pool).used);

    nad_al_pool_drop(pool);
}

static void test_exhaustion_returns_null() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 3);

    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_NOT_NULL(nad_alloc(pool, 32));
    }

    const nad_AlPoolStats st = nad_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(3, st.used);
    TEST_ASSERT_EQUAL_size_t(0, st.free);

    TEST_ASSERT_NULL(nad_alloc(pool, 32));
    TEST_ASSERT_EQUAL_size_t(3, nad_al_pool_stats(pool).used);

    nad_al_pool_drop(pool);
}

static void test_every_block_is_aligned() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 24, 4);

    for (size_t i = 0; i < 4; ++i) {
        void *p = nad_alloc(pool, 24);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) p % ALIGNMENT);
    }

    nad_al_pool_drop(pool);
}

// every block must be a distinct, non-overlapping region of the backing buffer
static void test_blocks_do_not_overlap() {
    constexpr size_t COUNT = 4;
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, COUNT);

    unsigned char *blocks[COUNT];
    for (size_t i = 0; i < COUNT; ++i) {
        blocks[i] = nad_alloc(pool, 32);
        TEST_ASSERT_NOT_NULL(blocks[i]);
        memset(blocks[i], (int) (i + 1), 32);
    }

    // if the slots overlapped, an earlier stamp would have been overwritten
    for (size_t i = 0; i < COUNT; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), blocks[i][0]);
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), blocks[i][31]);
    }

    nad_al_pool_drop(pool);
}

/* ========== dealloc ========== */

static void test_dealloc_returns_the_block() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 2);

    void *p = nad_alloc(pool, 32);
    TEST_ASSERT_EQUAL_size_t(1, nad_al_pool_stats(pool).used);

    nad_dealloc(pool, p, 32);

    const nad_AlPoolStats st = nad_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(0, st.used);
    TEST_ASSERT_EQUAL_size_t(2, st.free);

    nad_al_pool_drop(pool);
}

// the free list is LIFO: the block just returned is the next one handed out
static void test_freed_block_is_reused_first() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 3);

    void *a = nad_alloc(pool, 32);
    void *b = nad_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    nad_dealloc(pool, b, 32);
    TEST_ASSERT_EQUAL_PTR(b, nad_alloc(pool, 32));

    nad_al_pool_drop(pool);
}

// a fully drained pool becomes usable again once blocks come back
static void test_exhausted_pool_recovers_after_a_free() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 2);

    void *a = nad_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(nad_alloc(pool, 32));
    TEST_ASSERT_NULL(nad_alloc(pool, 32));

    nad_dealloc(pool, a, 32);
    TEST_ASSERT_EQUAL_PTR(a, nad_alloc(pool, 32));

    nad_al_pool_drop(pool);
}

static void test_dealloc_null_is_noop() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 2);

    TEST_ASSERT_NOT_NULL(nad_alloc(pool, 32));
    nad_dealloc(pool, nullptr, 32);
    TEST_ASSERT_EQUAL_size_t(1, nad_al_pool_stats(pool).used);

    nad_al_pool_drop(pool);
}

/* ========== calloc ========== */

static void test_calloc_zeroes_the_block() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 2);

    const unsigned char *p = nad_calloc(pool, 8, 4);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }

    nad_al_pool_drop(pool);
}

// a recycled block must not leak the previous tenant's bytes through calloc
static void test_calloc_zeroes_a_recycled_block() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 2);

    unsigned char *first = nad_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(first);
    memset(first, 0xFF, 32);
    nad_dealloc(pool, first, 32);

    const unsigned char *second = nad_calloc(pool, 8, 4);
    TEST_ASSERT_EQUAL_PTR(first, second);
    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, second[i]);
    }

    nad_al_pool_drop(pool);
}

static void test_calloc_larger_than_a_block_fails() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 2);

    TEST_ASSERT_NULL(nad_calloc(pool, 8, 8));
    TEST_ASSERT_EQUAL_size_t(0, nad_al_pool_stats(pool).used);

    nad_al_pool_drop(pool);
}

static void test_calloc_rejects_overflow() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 2);

    TEST_ASSERT_NULL(nad_calloc(pool, SIZE_MAX, 2));
    TEST_ASSERT_EQUAL_size_t(0, nad_al_pool_stats(pool).used);

    nad_al_pool_drop(pool);
}

/* ========== realloc ========== */

// the pool has no realloc hook; the fallback copies into a fresh block and
// returns the old one, so the net block count is unchanged
static void test_realloc_falls_back_to_a_fresh_block() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 3);

    unsigned char *p = nad_alloc(pool, 16);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 16; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    unsigned char *q = nad_realloc(pool, p, 16, 32);
    TEST_ASSERT_NOT_NULL(q);
    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), q[i]);
    }
    TEST_ASSERT_EQUAL_size_t(1, nad_al_pool_stats(pool).used);

    nad_al_pool_drop(pool);
}

/* ========== reset ========== */

static void test_reset_frees_every_block() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 3);

    void *first = nad_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(nad_alloc(pool, 32));
    TEST_ASSERT_NOT_NULL(nad_alloc(pool, 32));

    nad_al_pool_reset(pool);

    const nad_AlPoolStats st = nad_al_pool_stats(pool);
    TEST_ASSERT_EQUAL_size_t(0, st.used);
    TEST_ASSERT_EQUAL_size_t(3, st.free);

    // the free list is rebuilt in order, so the first block comes back first
    TEST_ASSERT_EQUAL_PTR(first, nad_alloc(pool, 32));

    nad_al_pool_drop(pool);
}

// reset after partial frees must rebuild the list, not append to a stale one
static void test_reset_recovers_from_a_scrambled_free_list() {
    nad_Al *pool = nad_al_pool_new(nad_al_default(), 32, 3);

    void *a = nad_alloc(pool, 32);
    void *b = nad_alloc(pool, 32);
    nad_dealloc(pool, a, 32);
    nad_dealloc(pool, b, 32);

    nad_al_pool_reset(pool);
    TEST_ASSERT_EQUAL_size_t(3, nad_al_pool_stats(pool).free);

    // all three blocks must still be reachable, and all distinct
    void *got[3];
    for (size_t i = 0; i < 3; ++i) {
        got[i] = nad_alloc(pool, 32);
        TEST_ASSERT_NOT_NULL(got[i]);
    }
    TEST_ASSERT_TRUE(got[0] != got[1]);
    TEST_ASSERT_TRUE(got[1] != got[2]);
    TEST_ASSERT_TRUE(got[0] != got[2]);

    nad_al_pool_drop(pool);
}

/* ========== composition ========== */

// a pool is an ordinary allocator, so an arena can back it
static void test_pool_can_live_in_another_allocator() {
    nad_Al parent = probe_al();

    nad_Al *pool = nad_al_pool_new(&parent, 32, 2);
    TEST_ASSERT_NOT_NULL(pool);

    unsigned char *p = nad_alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0x3C, 32);
    TEST_ASSERT_EQUAL_UINT8(0x3C, p[31]);

    nad_al_pool_drop(pool);
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
