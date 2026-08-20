#include "nad/alloc/arena.h"
#include "nad/alloc/default.h"

#include "unity.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// the arena rounds every request up to this, so sizes and offsets are predictable
static constexpr size_t ALIGNMENT = alignof(max_align_t);

static size_t aligned(size_t size) {
    return (size + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
}

/* ========== probe allocator ==========
 *
 * Counts live blocks and can fail on demand, so the arena's own construction
 * and teardown paths can be checked for leaks.
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

static void test_new_starts_empty() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);
    TEST_ASSERT_NOT_NULL(arena);

    const nad_AlArenaStats st = nad_al_arena_stats(arena);
    TEST_ASSERT_EQUAL_size_t(1024, st.cap);
    TEST_ASSERT_EQUAL_size_t(0, st.used);
    TEST_ASSERT_EQUAL_size_t(1024, st.available);

    nad_al_arena_drop(arena);
}

// the arena borrows its parent: everything it took must go back on drop
static void test_drop_returns_everything_to_the_parent() {
    nad_Al parent = probe_al();

    nad_Al *arena = nad_al_arena_new(&parent, 256);
    TEST_ASSERT_NOT_NULL(arena);
    TEST_ASSERT_EQUAL_size_t(3, probe.live); // buffer, context, the nad_Al itself

    nad_al_arena_drop(arena);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// each construction step can fail; none of them may leak the earlier ones
static void test_new_cleans_up_after_a_failing_parent() {
    for (size_t fail_at = 0; fail_at < 3; ++fail_at) {
        probe = (Probe){.fail_after = fail_at};
        nad_Al parent = probe_al();

        TEST_ASSERT_NULL(nad_al_arena_new(&parent, 256));
        TEST_ASSERT_EQUAL_size_t(0, probe.live);
    }
}

static void test_drop_null_is_noop() {
    nad_al_arena_drop(nullptr);
}

/* ========== alloc ========== */

static void test_alloc_advances_by_the_aligned_size() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);

    void *p = nad_alloc(arena, 1);
    TEST_ASSERT_NOT_NULL(p);

    const nad_AlArenaStats st = nad_al_arena_stats(arena);
    TEST_ASSERT_EQUAL_size_t(aligned(1), st.used);
    TEST_ASSERT_EQUAL_size_t(1024 - aligned(1), st.available);

    nad_al_arena_drop(arena);
}

static void test_every_block_is_aligned() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);

    for (size_t size = 1; size <= 40; size += 7) {
        void *p = nad_alloc(arena, size);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) p % ALIGNMENT);
    }

    nad_al_arena_drop(arena);
}

// consecutive blocks are disjoint — writing one must not disturb its neighbour
static void test_blocks_do_not_overlap() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);

    unsigned char *a = nad_alloc(arena, 16);
    unsigned char *b = nad_alloc(arena, 16);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_TRUE(a != b);

    memset(a, 0x11, 16);
    memset(b, 0x22, 16);
    TEST_ASSERT_EQUAL_UINT8(0x11, a[15]);
    TEST_ASSERT_EQUAL_UINT8(0x22, b[0]);

    nad_al_arena_drop(arena);
}

static void test_alloc_zero_is_null_and_does_not_move_the_offset() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);

    TEST_ASSERT_NULL(nad_alloc(arena, 0));
    TEST_ASSERT_EQUAL_size_t(0, nad_al_arena_stats(arena).used);

    nad_al_arena_drop(arena);
}

// exhaustion is a runtime state, not a bug: it returns nullptr and changes nothing
static void test_alloc_beyond_the_capacity_fails() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 64);

    TEST_ASSERT_NULL(nad_alloc(arena, 65));
    TEST_ASSERT_EQUAL_size_t(0, nad_al_arena_stats(arena).used);

    // a request that still fits is unaffected by the failed one
    TEST_ASSERT_NOT_NULL(nad_alloc(arena, 16));

    nad_al_arena_drop(arena);
}

static void test_arena_can_be_filled_exactly() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 4 * ALIGNMENT);

    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_NOT_NULL(nad_alloc(arena, ALIGNMENT));
    }

    const nad_AlArenaStats st = nad_al_arena_stats(arena);
    TEST_ASSERT_EQUAL_size_t(4 * ALIGNMENT, st.used);
    TEST_ASSERT_EQUAL_size_t(0, st.available);

    // and the next one has nowhere to go
    TEST_ASSERT_NULL(nad_alloc(arena, 1));

    nad_al_arena_drop(arena);
}

// rounding up must be accounted for: a small request still consumes a whole slot
static void test_capacity_accounts_for_rounding() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), ALIGNMENT);

    TEST_ASSERT_NOT_NULL(nad_alloc(arena, 1));
    TEST_ASSERT_EQUAL_size_t(0, nad_al_arena_stats(arena).available);
    TEST_ASSERT_NULL(nad_alloc(arena, 1));

    nad_al_arena_drop(arena);
}

/* ========== calloc ========== */

static void test_calloc_zeroes_the_block() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);

    const unsigned char *p = nad_calloc(arena, 8, 4);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }

    nad_al_arena_drop(arena);
}

// a reused arena must not hand out the previous tenant's bytes through calloc
static void test_calloc_zeroes_reused_memory() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);

    unsigned char *first = nad_alloc(arena, 32);
    TEST_ASSERT_NOT_NULL(first);
    memset(first, 0xFF, 32);

    nad_al_arena_reset(arena);

    const unsigned char *second = nad_calloc(arena, 8, 4);
    TEST_ASSERT_EQUAL_PTR(first, second);
    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, second[i]);
    }

    nad_al_arena_drop(arena);
}

static void test_calloc_rejects_overflow() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);

    TEST_ASSERT_NULL(nad_calloc(arena, SIZE_MAX, 2));
    TEST_ASSERT_EQUAL_size_t(0, nad_al_arena_stats(arena).used);

    nad_al_arena_drop(arena);
}

/* ========== realloc ========== */

// the arena has no realloc hook, so nad_realloc falls back to alloc + copy
static void test_realloc_falls_back_to_a_fresh_block() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 1024);

    unsigned char *p = nad_alloc(arena, 16);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 16; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    unsigned char *q = nad_realloc(arena, p, 16, 64);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE(p != q);
    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), q[i]);
    }

    // the old block is not reclaimed — both slots are still charged
    TEST_ASSERT_EQUAL_size_t(16 + 64, nad_al_arena_stats(arena).used);

    nad_al_arena_drop(arena);
}

/* ========== dealloc / reset ========== */

// individual frees are deliberately no-ops: only reset reclaims space
static void test_dealloc_does_not_reclaim() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);

    void *p = nad_alloc(arena, 32);
    const size_t used = nad_al_arena_stats(arena).used;

    nad_dealloc(arena, p, 32);
    TEST_ASSERT_EQUAL_size_t(used, nad_al_arena_stats(arena).used);

    nad_al_arena_drop(arena);
}

static void test_reset_reclaims_everything() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 256);

    void *first = nad_alloc(arena, 32);
    TEST_ASSERT_NOT_NULL(nad_alloc(arena, 32));

    nad_al_arena_reset(arena);

    const nad_AlArenaStats st = nad_al_arena_stats(arena);
    TEST_ASSERT_EQUAL_size_t(0, st.used);
    TEST_ASSERT_EQUAL_size_t(256, st.available);

    // the arena hands out the same memory again
    TEST_ASSERT_EQUAL_PTR(first, nad_alloc(arena, 32));

    nad_al_arena_drop(arena);
}

static void test_reset_of_an_untouched_arena_is_harmless() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 128);

    nad_al_arena_reset(arena);
    TEST_ASSERT_EQUAL_size_t(0, nad_al_arena_stats(arena).used);

    nad_al_arena_drop(arena);
}

/* ========== composition ========== */

// an arena is an ordinary allocator, so it can serve as another arena's parent
static void test_arena_can_feed_another_arena() {
    nad_Al *outer = nad_al_arena_new(nad_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(outer);

    nad_Al *inner = nad_al_arena_new(outer, 256);
    TEST_ASSERT_NOT_NULL(inner);

    unsigned char *p = nad_alloc(inner, 64);
    TEST_ASSERT_NOT_NULL(p);
    memset(p, 0x7E, 64);
    TEST_ASSERT_EQUAL_UINT8(0x7E, p[63]);

    TEST_ASSERT_EQUAL_size_t(256, nad_al_arena_stats(inner).cap);

    nad_al_arena_drop(inner);
    nad_al_arena_drop(outer);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_starts_empty);
    RUN_TEST(test_drop_returns_everything_to_the_parent);
    RUN_TEST(test_new_cleans_up_after_a_failing_parent);
    RUN_TEST(test_drop_null_is_noop);

    RUN_TEST(test_alloc_advances_by_the_aligned_size);
    RUN_TEST(test_every_block_is_aligned);
    RUN_TEST(test_blocks_do_not_overlap);
    RUN_TEST(test_alloc_zero_is_null_and_does_not_move_the_offset);
    RUN_TEST(test_alloc_beyond_the_capacity_fails);
    RUN_TEST(test_arena_can_be_filled_exactly);
    RUN_TEST(test_capacity_accounts_for_rounding);

    RUN_TEST(test_calloc_zeroes_the_block);
    RUN_TEST(test_calloc_zeroes_reused_memory);
    RUN_TEST(test_calloc_rejects_overflow);

    RUN_TEST(test_realloc_falls_back_to_a_fresh_block);

    RUN_TEST(test_dealloc_does_not_reclaim);
    RUN_TEST(test_reset_reclaims_everything);
    RUN_TEST(test_reset_of_an_untouched_arena_is_harmless);

    RUN_TEST(test_arena_can_feed_another_arena);

    return UNITY_END();
}
