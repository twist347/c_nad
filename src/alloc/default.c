#include "tda/alloc/default.h"

#include "tda/core/util.h"

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

tda_Al *tda_al_default() {
    static tda_Al static_al = {
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
    TDA_UNUSED(ctx);

    return malloc(size);
}

static void *calloc_wrapper(void *ctx, size_t num, size_t size) {
    TDA_UNUSED(ctx);

    return calloc(num, size);
}

static void *realloc_wrapper(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    TDA_UNUSED(ctx);
    TDA_UNUSED(old_size);

    return realloc(ptr, new_size);
}

static void free_wrapper(void *ctx, void *ptr, size_t size) {
    TDA_UNUSED(ctx);
    TDA_UNUSED(size);

    free(ptr);
}
