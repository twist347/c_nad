#pragma once

#include "nad/alloc/alloc.h"

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
} nad_TestProbe;

/* ========== info ========== */

/// every request that may hand out memory, in one number
[[nodiscard]]
static inline size_t nad_test_probe_requests(const nad_TestProbe *self) {
    assert(self);

    return self->alloc_calls + self->calloc_calls + self->realloc_calls;
}

/* ========== mods ========== */

static inline void nad_test_probe_reset(nad_TestProbe *self) {
    assert(self);

    *self = (nad_TestProbe){.fail_after = SIZE_MAX};
}

/// lets the next 'n' requests through and refuses the one after them
static inline void nad_test_probe_fail_after_next(nad_TestProbe *self, size_t n) {
    assert(self);

    self->fail_after = nad_test_probe_requests(self) + n;
}

/* ========== hooks ========== */

[[nodiscard]]
static inline bool nad_test_probe_refuses_(nad_TestProbe *self) {
    return nad_test_probe_requests(self) > self->fail_after;
}

[[nodiscard]]
static inline void *nad_test_probe_alloc_(void *ctx, size_t size) {
    nad_TestProbe *self = ctx;
    ++self->alloc_calls;
    self->last_alloc_size = size;

    if (nad_test_probe_refuses_(self)) {
        return nullptr;
    }

    void *ptr = malloc(size);
    if (ptr) {
        ++self->live;
    }
    return ptr;
}

[[nodiscard]]
static inline void *nad_test_probe_calloc_(void *ctx, size_t num, size_t size) {
    nad_TestProbe *self = ctx;
    ++self->calloc_calls;
    self->last_alloc_size = num * size;

    if (nad_test_probe_refuses_(self)) {
        return nullptr;
    }

    void *ptr = calloc(num, size);
    if (ptr) {
        ++self->live;
    }
    return ptr;
}

[[nodiscard]]
static inline void *nad_test_probe_realloc_(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    nad_TestProbe *self = ctx;
    ++self->realloc_calls;
    (void) old_size;

    // the wrapper never passes new_size == 0 down here
    if (nad_test_probe_refuses_(self)) {
        return nullptr; // ptr stays valid, as nad_realloc promises
    }

    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr && !ptr) {
        ++self->live; // grew out of nothing, so it is a new block
    }
    return new_ptr;
}

static inline void nad_test_probe_dealloc_(void *ctx, void *ptr, size_t size) {
    nad_TestProbe *self = ctx;
    assert(ptr); // the wrapper filters nullptr out

    ++self->dealloc_calls;
    self->last_dealloc_size = size;
    --self->live;

    free(ptr);
}

/* ========== allocators ========== */

/// no calloc, no realloc — nad_calloc/nad_realloc must synthesize them
[[nodiscard]]
static inline nad_Al nad_test_probe_bare(nad_TestProbe *self) {
    assert(self);

    return (nad_Al){
        .ctx = self,
        .alloc = nad_test_probe_alloc_,
        .calloc = nullptr,
        .realloc = nullptr,
        .dealloc = nad_test_probe_dealloc_,
    };
}

/// all four hooks — the wrappers must prefer these over their fallbacks
[[nodiscard]]
static inline nad_Al nad_test_probe_full(nad_TestProbe *self) {
    assert(self);

    return (nad_Al){
        .ctx = self,
        .alloc = nad_test_probe_alloc_,
        .calloc = nad_test_probe_calloc_,
        .realloc = nad_test_probe_realloc_,
        .dealloc = nad_test_probe_dealloc_,
    };
}
