#include "nad/alloc/alloc.h"

#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ========== probe allocator ==========
 *
 * A counting allocator over malloc that can be told to fail on demand. Two
 * flavours are built from it: a "bare" one exposing only alloc/dealloc, which
 * forces nad_calloc/nad_realloc down their fallback paths, and a "full" one
 * whose own calloc/realloc must be preferred over those fallbacks.
 */

typedef struct {
    size_t alloc_calls;
    size_t calloc_calls;
    size_t realloc_calls;
    size_t dealloc_calls;
    size_t live;
    size_t last_alloc_size;
    size_t last_dealloc_size;
    size_t fail_after; // number of successful allocs before failing
} Probe;

static Probe probe;

static void probe_reset() {
    probe = (Probe){.fail_after = SIZE_MAX};
}

static void *probe_alloc(void *ctx, size_t size) {
    Probe *p = ctx;
    ++p->alloc_calls;
    p->last_alloc_size = size;

    if (p->alloc_calls > p->fail_after) {
        return nullptr;
    }

    void *ptr = malloc(size);
    if (ptr) {
        ++p->live;
    }
    return ptr;
}

static void *probe_calloc(void *ctx, size_t num, size_t size) {
    Probe *p = ctx;
    ++p->calloc_calls;

    void *ptr = calloc(num, size);
    if (ptr) {
        ++p->live;
    }
    return ptr;
}

static void *probe_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    Probe *p = ctx;
    ++p->realloc_calls;
    (void) old_size;

    return realloc(ptr, new_size);
}

static void probe_dealloc(void *ctx, void *ptr, size_t size) {
    Probe *p = ctx;
    ++p->dealloc_calls;
    p->last_dealloc_size = size;
    --p->live;

    free(ptr);
}

// no calloc, no realloc — nad_calloc/nad_realloc must synthesize them
static nad_Al bare_al() {
    return (nad_Al){
        .ctx = &probe,
        .alloc = probe_alloc,
        .calloc = nullptr,
        .realloc = nullptr,
        .dealloc = probe_dealloc,
    };
}

static nad_Al full_al() {
    return (nad_Al){
        .ctx = &probe,
        .alloc = probe_alloc,
        .calloc = probe_calloc,
        .realloc = probe_realloc,
        .dealloc = probe_dealloc,
    };
}

void setUp() {
    probe_reset();
}

void tearDown() {
}

/* ========== alloc ========== */

static void test_alloc_returns_usable_memory() {
    nad_Al al = bare_al();

    unsigned char *p = nad_alloc(&al, 32);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(1, probe.alloc_calls);
    TEST_ASSERT_EQUAL_size_t(32, probe.last_alloc_size);

    memset(p, 0xAB, 32);
    TEST_ASSERT_EQUAL_UINT8(0xAB, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAB, p[31]);

    nad_dealloc(&al, p, 32);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

// a zero-size request is defined as nullptr and must not reach the allocator
static void test_alloc_zero_is_null_and_does_not_dispatch() {
    nad_Al al = bare_al();

    TEST_ASSERT_NULL(nad_alloc(&al, 0));
    TEST_ASSERT_EQUAL_size_t(0, probe.alloc_calls);
}

static void test_alloc_propagates_failure() {
    nad_Al al = bare_al();
    probe.fail_after = 0;

    TEST_ASSERT_NULL(nad_alloc(&al, 16));
    TEST_ASSERT_EQUAL_size_t(1, probe.alloc_calls);
}

/* ========== dealloc ========== */

static void test_dealloc_null_does_not_dispatch() {
    nad_Al al = bare_al();

    nad_dealloc(&al, nullptr, 16);
    TEST_ASSERT_EQUAL_size_t(0, probe.dealloc_calls);
}

// the size travels through untouched — the allocator is entitled to rely on it
static void test_dealloc_forwards_the_size() {
    nad_Al al = bare_al();

    void *p = nad_alloc(&al, 24);
    nad_dealloc(&al, p, 24);

    TEST_ASSERT_EQUAL_size_t(1, probe.dealloc_calls);
    TEST_ASSERT_EQUAL_size_t(24, probe.last_dealloc_size);
}

/* ========== calloc ========== */

static void test_calloc_zero_operand_is_null_and_does_not_dispatch() {
    nad_Al al = full_al();

    TEST_ASSERT_NULL(nad_calloc(&al, 0, 4));
    TEST_ASSERT_NULL(nad_calloc(&al, 4, 0));
    TEST_ASSERT_EQUAL_size_t(0, probe.calloc_calls);
    TEST_ASSERT_EQUAL_size_t(0, probe.alloc_calls);
}

// with a native calloc present, the fallback must not be used
static void test_calloc_prefers_the_native_hook() {
    nad_Al al = full_al();

    unsigned char *p = nad_calloc(&al, 4, 8);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(1, probe.calloc_calls);
    TEST_ASSERT_EQUAL_size_t(0, probe.alloc_calls);

    nad_dealloc(&al, p, 32);
}

// without one, nad_calloc synthesizes it from alloc + memset
static void test_calloc_fallback_zeroes_the_block() {
    nad_Al al = bare_al();

    unsigned char *p = nad_calloc(&al, 4, 8);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(1, probe.alloc_calls);
    TEST_ASSERT_EQUAL_size_t(32, probe.last_alloc_size);

    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }

    nad_dealloc(&al, p, 32);
}

// num * size overflowing size_t is caught before anything is requested
static void test_calloc_fallback_rejects_overflow() {
    nad_Al al = bare_al();

    TEST_ASSERT_NULL(nad_calloc(&al, SIZE_MAX, 2));
    TEST_ASSERT_EQUAL_size_t(0, probe.alloc_calls);
}

static void test_calloc_fallback_propagates_failure() {
    nad_Al al = bare_al();
    probe.fail_after = 0;

    TEST_ASSERT_NULL(nad_calloc(&al, 4, 8));
    TEST_ASSERT_EQUAL_size_t(1, probe.alloc_calls);
}

/* ========== realloc ========== */

static void test_realloc_prefers_the_native_hook() {
    nad_Al al = full_al();

    void *p = nad_alloc(&al, 16);
    void *q = nad_realloc(&al, p, 16, 32);

    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(1, probe.realloc_calls);
    TEST_ASSERT_EQUAL_size_t(1, probe.alloc_calls);

    free(q);
    probe.live = 0;
}

// new_size == 0 means "release it" — defined behaviour, not a failure
static void test_realloc_to_zero_releases() {
    nad_Al al = bare_al();

    void *p = nad_alloc(&al, 16);
    TEST_ASSERT_NULL(nad_realloc(&al, p, 16, 0));

    TEST_ASSERT_EQUAL_size_t(1, probe.dealloc_calls);
    TEST_ASSERT_EQUAL_size_t(16, probe.last_dealloc_size);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_realloc_fallback_grows_and_keeps_the_contents() {
    nad_Al al = bare_al();

    unsigned char *p = nad_alloc(&al, 8);
    for (size_t i = 0; i < 8; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    unsigned char *q = nad_realloc(&al, p, 8, 32);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < 8; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), q[i]);
    }

    // the old block is handed back with its original size
    TEST_ASSERT_EQUAL_size_t(1, probe.dealloc_calls);
    TEST_ASSERT_EQUAL_size_t(8, probe.last_dealloc_size);
    TEST_ASSERT_EQUAL_size_t(1, probe.live);

    nad_dealloc(&al, q, 32);
}

// shrinking copies only what fits, and must not read past the old block
static void test_realloc_fallback_shrinks() {
    nad_Al al = bare_al();

    unsigned char *p = nad_alloc(&al, 16);
    for (size_t i = 0; i < 16; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    unsigned char *q = nad_realloc(&al, p, 16, 4);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(4, probe.last_alloc_size);

    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), q[i]);
    }

    nad_dealloc(&al, q, 4);
}

// growing from nothing is just an allocation — there is nothing to release
static void test_realloc_fallback_from_null_is_an_alloc() {
    nad_Al al = bare_al();

    void *p = nad_realloc(&al, nullptr, 0, 16);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(1, probe.alloc_calls);
    TEST_ASSERT_EQUAL_size_t(0, probe.dealloc_calls);

    nad_dealloc(&al, p, 16);
}

// on failure the original block must survive intact — the caller still owns it
static void test_realloc_fallback_failure_keeps_the_original() {
    nad_Al al = bare_al();

    unsigned char *p = nad_alloc(&al, 8);
    for (size_t i = 0; i < 8; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    probe.fail_after = 1; // the first alloc already happened
    TEST_ASSERT_NULL(nad_realloc(&al, p, 8, 64));

    TEST_ASSERT_EQUAL_size_t(0, probe.dealloc_calls);
    TEST_ASSERT_EQUAL_size_t(1, probe.live);
    for (size_t i = 0; i < 8; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), p[i]);
    }

    probe.fail_after = SIZE_MAX;
    nad_dealloc(&al, p, 8);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_alloc_returns_usable_memory);
    RUN_TEST(test_alloc_zero_is_null_and_does_not_dispatch);
    RUN_TEST(test_alloc_propagates_failure);

    RUN_TEST(test_dealloc_null_does_not_dispatch);
    RUN_TEST(test_dealloc_forwards_the_size);

    RUN_TEST(test_calloc_zero_operand_is_null_and_does_not_dispatch);
    RUN_TEST(test_calloc_prefers_the_native_hook);
    RUN_TEST(test_calloc_fallback_zeroes_the_block);
    RUN_TEST(test_calloc_fallback_rejects_overflow);
    RUN_TEST(test_calloc_fallback_propagates_failure);

    RUN_TEST(test_realloc_prefers_the_native_hook);
    RUN_TEST(test_realloc_to_zero_releases);
    RUN_TEST(test_realloc_fallback_grows_and_keeps_the_contents);
    RUN_TEST(test_realloc_fallback_shrinks);
    RUN_TEST(test_realloc_fallback_from_null_is_an_alloc);
    RUN_TEST(test_realloc_fallback_failure_keeps_the_original);

    return UNITY_END();
}
