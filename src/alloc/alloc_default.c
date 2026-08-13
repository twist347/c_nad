#include "nad/alloc/alloc_default.h"
#include "nad/core/util.h"

#include <stddef.h>
#include <stdlib.h>

/* ========== internals ========== */

[[nodiscard]] static
void *malloc_wrapper(void *ctx, size_t size);

[[nodiscard]] static
void *calloc_wrapper(void *ctx, size_t num, size_t size);

[[nodiscard]] static
void *realloc_wrapper(void *ctx, void *ptr, size_t old_size, size_t new_size);

static
void free_wrapper(void *ctx, void *ptr, size_t size);

/* ========== lifetime ========== */

nad_Al *nad_al_default() {
    static nad_Al static_al = {
        .ctx = nullptr,
        .alloc = malloc_wrapper,
        .calloc = calloc_wrapper,
        .realloc = realloc_wrapper,
        .dealloc = free_wrapper,
    };

    return &static_al;
}

/* ========== internals ========== */

static
void *malloc_wrapper(void *ctx, size_t size) {
    NAD_UNUSED(ctx);

    return malloc(size);
}

static
void *calloc_wrapper(void *ctx, size_t num, size_t size) {
    NAD_UNUSED(ctx);

    return calloc(num, size);
}

static
void *realloc_wrapper(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    NAD_UNUSED(ctx);
    NAD_UNUSED(old_size);

    return realloc(ptr, new_size);
}

static
void free_wrapper(void *ctx, void *ptr, size_t size) {
    NAD_UNUSED(ctx);
    NAD_UNUSED(size);

    free(ptr);
}
