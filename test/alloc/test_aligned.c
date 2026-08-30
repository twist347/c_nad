#include "nad/alloc/aligned.h"
#include "nad/alloc/arena.h"
#include "nad/alloc/default.h"

#include "support/probe.h"

#include "unity.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ========== probe allocator ==========
 *
 * The shared probe (test/support/probe.h) in its "full" flavour: its own calloc and
 * realloc are there so the tests can watch that the aligned allocator never reaches
 * for them — either one would hand back a block aligned only for the parent.
 */

static nad_TestProbe probe;

static nad_Al probe_al() {
    return nad_test_probe_full(&probe);
}

// what a request for 'size' costs the parent
static size_t parent_size(size_t size, size_t alignment) {
    return size + alignment - 1 + sizeof(void *);
}

void setUp() {
    nad_test_probe_reset(&probe);
}

void tearDown() {
}

/* ========== lifetime ========== */

static void test_new_takes_two_blocks_and_drop_returns_them() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);
    TEST_ASSERT_EQUAL_size_t(2, probe.live);

    nad_al_aligned_drop(al);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// the ctx is taken first, so a parent that refuses either request must leak nothing
static void test_new_cleans_up_after_a_failing_parent() {
    nad_Al parent = probe_al();

    nad_test_probe_fail_after_next(&probe, 0);
    TEST_ASSERT_NULL(nad_al_aligned_new(&parent, 64));
    TEST_ASSERT_EQUAL_size_t(0, probe.live);

    nad_test_probe_reset(&probe);
    nad_test_probe_fail_after_next(&probe, 1);
    TEST_ASSERT_NULL(nad_al_aligned_new(&parent, 64));
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_drop_null_is_noop() {
    nad_al_aligned_drop(nullptr);
}

/* ========== alloc ========== */

static void test_every_block_is_aligned() {
    static constexpr size_t alignments[] = {64, 128, 256};

    for (size_t a = 0; a < sizeof(alignments) / sizeof(alignments[0]); ++a) {
        nad_Al *al = nad_al_aligned_new(nad_al_default(), alignments[a]);
        TEST_ASSERT_NOT_NULL(al);

        // walk the sizes across a whole period, so every offset the parent can
        // return is exercised
        for (size_t size = 1; size <= 2 * alignments[a]; ++size) {
            unsigned char *ptr = nad_alloc(al, size);
            TEST_ASSERT_NOT_NULL(ptr);
            TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) ptr % alignments[a]);

            memset(ptr, 0xAB, size); // the whole span has to be ours
            nad_dealloc(al, ptr, size);
        }

        nad_al_aligned_drop(al);
    }
}

static void test_blocks_do_not_overlap() {
    nad_Al *al = nad_al_aligned_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(al);

    unsigned char *a = nad_alloc(al, 100);
    unsigned char *b = nad_alloc(al, 100);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    memset(a, 0x11, 100);
    memset(b, 0x22, 100);
    for (size_t i = 0; i < 100; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0x11, a[i]);
    }

    nad_dealloc(al, a, 100);
    nad_dealloc(al, b, 100);
    nad_al_aligned_drop(al);
}

// the parent is asked for the padding and the word before the block, and gets the
// same number back on the way out — that identity is what keeps the sized dealloc honest
static void test_the_parent_sees_the_padded_size_both_ways() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);

    void *ptr = nad_alloc(al, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_size_t(parent_size(100, 64), probe.last_alloc_size);

    nad_dealloc(al, ptr, 100);
    TEST_ASSERT_EQUAL_size_t(probe.last_alloc_size, probe.last_dealloc_size);

    nad_al_aligned_drop(al);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_alloc_zero_is_null_and_asks_the_parent_nothing() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);

    const size_t before = nad_test_probe_requests(&probe);
    TEST_ASSERT_NULL(nad_alloc(al, 0));
    TEST_ASSERT_EQUAL_size_t(before, nad_test_probe_requests(&probe));

    nad_al_aligned_drop(al);
}

static void test_alloc_rejects_a_size_the_padding_would_overflow() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);

    const size_t before = nad_test_probe_requests(&probe);
    TEST_ASSERT_NULL(nad_alloc(al, SIZE_MAX));
    TEST_ASSERT_NULL(nad_alloc(al, SIZE_MAX - sizeof(void *)));
    TEST_ASSERT_EQUAL_size_t(before, nad_test_probe_requests(&probe));

    nad_al_aligned_drop(al);
}

static void test_alloc_fails_when_the_parent_does() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);

    nad_test_probe_fail_after_next(&probe, 0);
    TEST_ASSERT_NULL(nad_alloc(al, 100));
    TEST_ASSERT_EQUAL_size_t(2, probe.live); // still just the allocator's own two

    nad_test_probe_reset(&probe);
    nad_al_aligned_drop(al);
}

/* ========== dealloc ========== */

static void test_dealloc_null_is_noop() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);

    const size_t before = probe.dealloc_calls;
    nad_dealloc(al, nullptr, 0);
    TEST_ASSERT_EQUAL_size_t(before, probe.dealloc_calls);

    nad_al_aligned_drop(al);
}

/* ========== calloc and realloc ========== */

// no calloc of its own, so nad_calloc must build one on top of this alloc — never
// on the parent's, which would align the block to the parent instead
static void test_calloc_zeroes_an_aligned_block_without_the_parents_calloc() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);

    unsigned char *ptr = nad_calloc(al, 8, 16);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) ptr % 64);
    TEST_ASSERT_EQUAL_size_t(0, probe.calloc_calls);
    for (size_t i = 0; i < 8 * 16; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, ptr[i]);
    }

    nad_dealloc(al, ptr, 8 * 16);
    nad_al_aligned_drop(al);
}

// same for realloc: the parent's would keep the bytes but not the alignment
static void test_realloc_keeps_the_bytes_and_the_alignment() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);

    unsigned char *ptr = nad_alloc(al, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 0x5A, 100);

    unsigned char *grown = nad_realloc(al, ptr, 100, 500);
    TEST_ASSERT_NOT_NULL(grown);
    TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) grown % 64);
    TEST_ASSERT_EQUAL_size_t(0, probe.realloc_calls);
    for (size_t i = 0; i < 100; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0x5A, grown[i]);
    }

    unsigned char *shrunk = nad_realloc(al, grown, 500, 50);
    TEST_ASSERT_NOT_NULL(shrunk);
    TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) shrunk % 64);
    for (size_t i = 0; i < 50; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0x5A, shrunk[i]);
    }

    nad_dealloc(al, shrunk, 50);
    nad_al_aligned_drop(al);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_realloc_to_zero_gives_the_block_back() {
    nad_Al parent = probe_al();

    nad_Al *al = nad_al_aligned_new(&parent, 64);
    TEST_ASSERT_NOT_NULL(al);

    void *ptr = nad_alloc(al, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_size_t(3, probe.live);

    TEST_ASSERT_NULL(nad_realloc(al, ptr, 100, 0));
    TEST_ASSERT_EQUAL_size_t(2, probe.live);

    nad_al_aligned_drop(al);
}

/* ========== nesting ========== */

// alignment is a property of the allocator, so it composes: the arena's own step is
// max_align_t and the wrapper lifts what it hands out to 128
static void test_over_an_arena() {
    nad_Al *arena = nad_al_arena_new(nad_al_default(), 4096);
    TEST_ASSERT_NOT_NULL(arena);

    nad_Al *al = nad_al_aligned_new(arena, 128);
    TEST_ASSERT_NOT_NULL(al);

    for (size_t i = 0; i < 4; ++i) {
        void *ptr = nad_alloc(al, 100);
        TEST_ASSERT_NOT_NULL(ptr);
        TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) ptr % 128);
    }

    nad_al_aligned_drop(al);
    nad_al_arena_drop(arena);
}

static void test_over_itself() {
    nad_Al *inner = nad_al_aligned_new(nad_al_default(), 64);
    TEST_ASSERT_NOT_NULL(inner);

    nad_Al *outer = nad_al_aligned_new(inner, 256);
    TEST_ASSERT_NOT_NULL(outer);

    void *ptr = nad_alloc(outer, 300);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) ptr % 256);

    nad_dealloc(outer, ptr, 300);
    nad_al_aligned_drop(outer);
    nad_al_aligned_drop(inner);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_takes_two_blocks_and_drop_returns_them);
    RUN_TEST(test_new_cleans_up_after_a_failing_parent);
    RUN_TEST(test_drop_null_is_noop);

    RUN_TEST(test_every_block_is_aligned);
    RUN_TEST(test_blocks_do_not_overlap);
    RUN_TEST(test_the_parent_sees_the_padded_size_both_ways);
    RUN_TEST(test_alloc_zero_is_null_and_asks_the_parent_nothing);
    RUN_TEST(test_alloc_rejects_a_size_the_padding_would_overflow);
    RUN_TEST(test_alloc_fails_when_the_parent_does);

    RUN_TEST(test_dealloc_null_is_noop);

    RUN_TEST(test_calloc_zeroes_an_aligned_block_without_the_parents_calloc);
    RUN_TEST(test_realloc_keeps_the_bytes_and_the_alignment);
    RUN_TEST(test_realloc_to_zero_gives_the_block_back);

    RUN_TEST(test_over_an_arena);
    RUN_TEST(test_over_itself);

    return UNITY_END();
}
