#include "trs/alloc/default.h"

#include "trs/core/util.h"

#include <stddef.h>
#include <stdlib.h>

/* ========== internals ========== */

[[nodiscard]]
static void *malloc_wrapper(void *ctx, size_t size);

[[nodiscard]]
static void *calloc_wrapper(void *ctx, size_t num, size_t size);

[[nodiscard]]
static void *realloc_wrapper(void *ctx, void *ptr, size_t old_size, size_t new_size);

static void free_wrapper(void *ctx, void *ptr, size_t size);

/* ========== lifetime ========== */

trs_Al *trs_al_default() {
    static trs_Al static_al = {
        .ctx = nullptr,
        .alloc = malloc_wrapper,
        .calloc = calloc_wrapper,
        .realloc = realloc_wrapper,
        .dealloc = free_wrapper,
    };

    return &static_al;
}

/* ========== internals ========== */

static void *malloc_wrapper(void *ctx, size_t size) {
    TRS_UNUSED(ctx);

    return malloc(size);
}

static void *calloc_wrapper(void *ctx, size_t num, size_t size) {
    TRS_UNUSED(ctx);

    return calloc(num, size);
}

static void *realloc_wrapper(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    TRS_UNUSED(ctx);
    TRS_UNUSED(old_size);

    return realloc(ptr, new_size);
}

static void free_wrapper(void *ctx, void *ptr, size_t size) {
    TRS_UNUSED(ctx);
    TRS_UNUSED(size);

    free(ptr);
}
