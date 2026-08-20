#include "nad/alloc/default.h"

#include "unity.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp() {
}

void tearDown() {
}

/* ========== identity ========== */

// the default allocator is a static singleton — every call names the same object
static void test_default_is_a_singleton() {
    nad_Al *a = nad_al_default();
    nad_Al *b = nad_al_default();

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_PTR(a, b);
}

static void test_default_provides_every_hook() {
    const nad_Al *al = nad_al_default();

    TEST_ASSERT_NOT_NULL(al->alloc);
    TEST_ASSERT_NOT_NULL(al->calloc);
    TEST_ASSERT_NOT_NULL(al->realloc);
    TEST_ASSERT_NOT_NULL(al->dealloc);
}

/* ========== alloc ========== */

static void test_alloc_returns_usable_memory() {
    nad_Al *al = nad_al_default();

    unsigned char *p = nad_alloc(al, 64);
    TEST_ASSERT_NOT_NULL(p);

    memset(p, 0x5A, 64);
    TEST_ASSERT_EQUAL_UINT8(0x5A, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0x5A, p[63]);

    nad_dealloc(al, p, 64);
}

static void test_alloc_zero_is_null() {
    TEST_ASSERT_NULL(nad_alloc(nad_al_default(), 0));
}

// malloc's guarantee: suitably aligned for any fundamental type
static void test_alloc_is_max_aligned() {
    nad_Al *al = nad_al_default();

    for (size_t size = 1; size <= 64; size *= 2) {
        void *p = nad_alloc(al, size);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQUAL_size_t(0, (uintptr_t) p % alignof(max_align_t));
        nad_dealloc(al, p, size);
    }
}

// two live allocations are distinct regions — writing one must not touch the other
static void test_allocations_are_independent() {
    nad_Al *al = nad_al_default();

    unsigned char *a = nad_alloc(al, 16);
    unsigned char *b = nad_alloc(al, 16);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_TRUE(a != b);

    memset(a, 0x11, 16);
    memset(b, 0x22, 16);
    TEST_ASSERT_EQUAL_UINT8(0x11, a[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22, b[0]);

    nad_dealloc(al, b, 16);
    nad_dealloc(al, a, 16);
}

/* ========== calloc ========== */

static void test_calloc_zeroes_the_block() {
    nad_Al *al = nad_al_default();

    const unsigned char *p = nad_calloc(al, 8, 4);
    TEST_ASSERT_NOT_NULL(p);

    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }

    nad_dealloc(al, (void *) p, 32);
}

static void test_calloc_zero_operand_is_null() {
    nad_Al *al = nad_al_default();

    TEST_ASSERT_NULL(nad_calloc(al, 0, 4));
    TEST_ASSERT_NULL(nad_calloc(al, 4, 0));
}

// an overflowing request must fail, not wrap around into a small block
static void test_calloc_rejects_overflow() {
    TEST_ASSERT_NULL(nad_calloc(nad_al_default(), SIZE_MAX, 2));
}

/* ========== realloc ========== */

static void test_realloc_grows_and_keeps_the_contents() {
    nad_Al *al = nad_al_default();

    unsigned char *p = nad_alloc(al, 8);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 8; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    unsigned char *q = nad_realloc(al, p, 8, 64);
    TEST_ASSERT_NOT_NULL(q);
    for (size_t i = 0; i < 8; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), q[i]);
    }

    nad_dealloc(al, q, 64);
}

static void test_realloc_shrinks_and_keeps_the_prefix() {
    nad_Al *al = nad_al_default();

    unsigned char *p = nad_alloc(al, 32);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 32; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    unsigned char *q = nad_realloc(al, p, 32, 4);
    TEST_ASSERT_NOT_NULL(q);
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), q[i]);
    }

    nad_dealloc(al, q, 4);
}

static void test_realloc_to_zero_releases() {
    nad_Al *al = nad_al_default();

    void *p = nad_alloc(al, 16);
    TEST_ASSERT_NOT_NULL(p);

    TEST_ASSERT_NULL(nad_realloc(al, p, 16, 0));
}

static void test_realloc_from_null_is_an_alloc() {
    nad_Al *al = nad_al_default();

    void *p = nad_realloc(al, nullptr, 0, 16);
    TEST_ASSERT_NOT_NULL(p);

    nad_dealloc(al, p, 16);
}

/* ========== dealloc ========== */

static void test_dealloc_null_is_noop() {
    nad_dealloc(nad_al_default(), nullptr, 16);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_default_is_a_singleton);
    RUN_TEST(test_default_provides_every_hook);

    RUN_TEST(test_alloc_returns_usable_memory);
    RUN_TEST(test_alloc_zero_is_null);
    RUN_TEST(test_alloc_is_max_aligned);
    RUN_TEST(test_allocations_are_independent);

    RUN_TEST(test_calloc_zeroes_the_block);
    RUN_TEST(test_calloc_zero_operand_is_null);
    RUN_TEST(test_calloc_rejects_overflow);

    RUN_TEST(test_realloc_grows_and_keeps_the_contents);
    RUN_TEST(test_realloc_shrinks_and_keeps_the_prefix);
    RUN_TEST(test_realloc_to_zero_releases);
    RUN_TEST(test_realloc_from_null_is_an_alloc);

    RUN_TEST(test_dealloc_null_is_noop);

    return UNITY_END();
}
