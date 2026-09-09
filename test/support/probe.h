#pragma once

#include "terse/alloc/alloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

/* ========== type ========== */

/// counting allocator over malloc that can be told to fail on demand.
/// Every instance carries its own state — pass it as the al's ctx, so
/// two probes in one test never see each other's counters.
typedef struct {
    size_t alloc_calls;
    size_t calloc_calls;
    size_t realloc_calls;
    size_t dealloc_calls;
    size_t live; // blocks handed out and not yet freed
    size_t last_alloc_size;
    size_t last_dealloc_size;
    size_t fail_after; // requests served before the probe starts refusing
} trs_TestProbe;

/* ========== info ========== */

/// every request that may hand out memory, in one number
[[nodiscard]]
static inline size_t trs_test_probe_requests(const trs_TestProbe *self) {
    assert(self);

    return self->alloc_calls + self->calloc_calls + self->realloc_calls;
}

/* ========== mods ========== */

static inline void trs_test_probe_reset(trs_TestProbe *self) {
    assert(self);

    *self = (trs_TestProbe){.fail_after = SIZE_MAX};
}

/// lets the next 'n' requests through and refuses the one after them
static inline void trs_test_probe_fail_after_next(trs_TestProbe *self, size_t n) {
    assert(self);

    self->fail_after = trs_test_probe_requests(self) + n;
}

/* ========== hooks ========== */

[[nodiscard]]
static inline bool trs_test_probe_refuses_(trs_TestProbe *self) {
    return trs_test_probe_requests(self) > self->fail_after;
}

[[nodiscard]]
static inline void *trs_test_probe_alloc_(void *ctx, size_t size) {
    trs_TestProbe *self = ctx;
    ++self->alloc_calls;
    self->last_alloc_size = size;

    if (trs_test_probe_refuses_(self)) {
        return nullptr;
    }

    void *ptr = malloc(size);
    if (ptr) {
        ++self->live;
    }
    return ptr;
}

[[nodiscard]]
static inline void *trs_test_probe_calloc_(void *ctx, size_t num, size_t size) {
    trs_TestProbe *self = ctx;
    ++self->calloc_calls;
    self->last_alloc_size = num * size;

    if (trs_test_probe_refuses_(self)) {
        return nullptr;
    }

    void *ptr = calloc(num, size);
    if (ptr) {
        ++self->live;
    }
    return ptr;
}

[[nodiscard]]
static inline void *trs_test_probe_realloc_(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    trs_TestProbe *self = ctx;
    ++self->realloc_calls;
    (void) old_size;

    // the wrapper never passes new_size == 0 down here
    if (trs_test_probe_refuses_(self)) {
        return nullptr; // ptr stays valid, as trs_realloc promises
    }

    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr && !ptr) {
        ++self->live; // grew out of nothing, so it is a new block
    }
    return new_ptr;
}

static inline void trs_test_probe_dealloc_(void *ctx, void *ptr, size_t size) {
    trs_TestProbe *self = ctx;
    assert(ptr); // the wrapper filters nullptr out

    ++self->dealloc_calls;
    self->last_dealloc_size = size;
    --self->live;

    free(ptr);
}

/* ========== allocators ========== */

/// no calloc, no realloc — trs_calloc/trs_realloc must synthesize them
[[nodiscard]]
static inline trs_Al trs_test_probe_bare(trs_TestProbe *self) {
    assert(self);

    return (trs_Al){
        .ctx = self,
        .alloc = trs_test_probe_alloc_,
        .calloc = nullptr,
        .realloc = nullptr,
        .dealloc = trs_test_probe_dealloc_,
    };
}

/// all four hooks — the wrappers must prefer these over their fallbacks
[[nodiscard]]
static inline trs_Al trs_test_probe_full(trs_TestProbe *self) {
    assert(self);

    return (trs_Al){
        .ctx = self,
        .alloc = trs_test_probe_alloc_,
        .calloc = trs_test_probe_calloc_,
        .realloc = trs_test_probe_realloc_,
        .dealloc = trs_test_probe_dealloc_,
    };
}
