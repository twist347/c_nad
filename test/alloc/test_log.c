#include "nad/alloc/log.h"

#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== probe allocator ==========
 *
 * The wrapped allocator. Its counters prove that the log layer forwards every
 * operation instead of merely reporting it.
 */

typedef struct {
    size_t live;
    size_t alloc_calls;
    size_t calloc_calls;
    size_t realloc_calls;
    size_t dealloc_calls;
    size_t fail_after;
} Probe;

static Probe probe;
static FILE *log_stream;

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
    (void) size;

    --p->live;
    free(ptr);
}

static nad_Al probe_al() {
    return (nad_Al){
        .ctx = &probe,
        .alloc = probe_alloc,
        .calloc = probe_calloc,
        .realloc = probe_realloc,
        .dealloc = probe_dealloc,
    };
}

// snapshot of everything written to the log so far
static void log_text(char *buf, size_t cap) {
    fflush(log_stream);
    rewind(log_stream);

    const size_t n = fread(buf, 1, cap - 1, log_stream);
    buf[n] = '\0';

    fseek(log_stream, 0, SEEK_END);
}

void setUp() {
    probe = (Probe){.fail_after = SIZE_MAX};
    log_stream = tmpfile();
    TEST_ASSERT_NOT_NULL(log_stream);
}

void tearDown() {
    if (log_stream) {
        fclose(log_stream);
        log_stream = nullptr;
    }
}

/* ========== lifetime ========== */

static void test_new_announces_itself() {
    nad_Al parent = probe_al();

    nad_Al *log = nad_al_log_new(&parent, log_stream);
    TEST_ASSERT_NOT_NULL(log);

    char buf[512];
    log_text(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "log allocator created"));

    nad_al_log_drop(log);
}

static void test_new_provides_every_hook() {
    nad_Al parent = probe_al();

    const nad_Al *log = nad_al_log_new(&parent, log_stream);
    TEST_ASSERT_NOT_NULL(log);

    TEST_ASSERT_NOT_NULL(log->alloc);
    TEST_ASSERT_NOT_NULL(log->calloc);
    TEST_ASSERT_NOT_NULL(log->realloc);
    TEST_ASSERT_NOT_NULL(log->dealloc);

    nad_al_log_drop((nad_Al *) log);
}

// the wrapper borrows the allocator it decorates: its own memory comes from there
static void test_drop_returns_everything_to_the_wrapped() {
    nad_Al parent = probe_al();

    nad_Al *log = nad_al_log_new(&parent, log_stream);
    TEST_ASSERT_EQUAL_size_t(2, probe.live); // context and the nad_Al itself

    nad_al_log_drop(log);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);

    char buf[512];
    log_text(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "log allocator destroyed"));
}

static void test_new_cleans_up_after_a_failing_parent() {
    for (size_t fail_at = 0; fail_at < 2; ++fail_at) {
        probe = (Probe){.fail_after = fail_at};
        nad_Al parent = probe_al();

        TEST_ASSERT_NULL(nad_al_log_new(&parent, log_stream));
        TEST_ASSERT_EQUAL_size_t(0, probe.live);
    }
}

static void test_drop_null_is_noop() {
    nad_al_log_drop(nullptr);
}

/* ========== forwarding ========== */

// logging is a side effect — the memory itself must come from the wrapped allocator
static void test_alloc_is_forwarded_and_usable() {
    nad_Al parent = probe_al();
    nad_Al *log = nad_al_log_new(&parent, log_stream);

    // the wrapper itself allocated from the probe, so count from here
    const size_t base = probe.alloc_calls;

    unsigned char *p = nad_alloc(log, 32);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(base + 1, probe.alloc_calls);

    memset(p, 0x6D, 32);
    TEST_ASSERT_EQUAL_UINT8(0x6D, p[31]);

    nad_dealloc(log, p, 32);
    TEST_ASSERT_EQUAL_size_t(1, probe.dealloc_calls);

    nad_al_log_drop(log);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_calloc_is_forwarded() {
    nad_Al parent = probe_al();
    nad_Al *log = nad_al_log_new(&parent, log_stream);

    const unsigned char *p = nad_calloc(log, 4, 8);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(1, probe.calloc_calls);
    for (size_t i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, p[i]);
    }

    nad_dealloc(log, (void *) p, 32);
    nad_al_log_drop(log);
    TEST_ASSERT_EQUAL_size_t(0, probe.live);
}

static void test_realloc_is_forwarded_and_keeps_the_contents() {
    nad_Al parent = probe_al();
    nad_Al *log = nad_al_log_new(&parent, log_stream);

    unsigned char *p = nad_alloc(log, 16);
    TEST_ASSERT_NOT_NULL(p);
    for (size_t i = 0; i < 16; ++i) {
        p[i] = (unsigned char) (i + 1);
    }

    unsigned char *q = nad_realloc(log, p, 16, 64);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_size_t(1, probe.realloc_calls);
    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT_EQUAL_UINT8((unsigned char) (i + 1), q[i]);
    }

    nad_dealloc(log, q, 64);
    nad_al_log_drop(log);
}

// a failure in the wrapped allocator passes through unchanged
static void test_failure_is_forwarded() {
    nad_Al parent = probe_al();
    nad_Al *log = nad_al_log_new(&parent, log_stream);

    probe.fail_after = probe.alloc_calls; // fail from here on
    TEST_ASSERT_NULL(nad_alloc(log, 32));

    probe.fail_after = SIZE_MAX;
    nad_al_log_drop(log);
}

/* ========== the log itself ========== */

static void test_every_operation_is_recorded() {
    nad_Al parent = probe_al();
    nad_Al *log = nad_al_log_new(&parent, log_stream);

    void *p = nad_alloc(log, 32);
    void *q = nad_realloc(log, p, 32, 64);
    nad_dealloc(log, q, 64);

    void *c = nad_calloc(log, 4, 8);
    nad_dealloc(log, c, 32);

    char buf[2048];
    log_text(buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "alloc size = 32"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "calloc num = 4 size = 8"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "old size = 32 new_size = 64"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "dealloc"));

    nad_al_log_drop(log);
}

// the log is flushed as it goes, so it survives a crash before drop
static void test_the_log_is_flushed_eagerly() {
    nad_Al parent = probe_al();
    nad_Al *log = nad_al_log_new(&parent, log_stream);

    void *p = nad_alloc(log, 128);

    char buf[1024];
    log_text(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "alloc size = 128"));

    nad_dealloc(log, p, 128);
    nad_al_log_drop(log);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_new_announces_itself);
    RUN_TEST(test_new_provides_every_hook);
    RUN_TEST(test_drop_returns_everything_to_the_wrapped);
    RUN_TEST(test_new_cleans_up_after_a_failing_parent);
    RUN_TEST(test_drop_null_is_noop);

    RUN_TEST(test_alloc_is_forwarded_and_usable);
    RUN_TEST(test_calloc_is_forwarded);
    RUN_TEST(test_realloc_is_forwarded_and_keeps_the_contents);
    RUN_TEST(test_failure_is_forwarded);

    RUN_TEST(test_every_operation_is_recorded);
    RUN_TEST(test_the_log_is_flushed_eagerly);

    return UNITY_END();
}
