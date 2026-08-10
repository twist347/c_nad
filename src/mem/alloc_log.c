#include "nad/mem/alloc_log.h"
#include "nad/mem/alloc.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static void *log_alloc(void *ctx, size_t size);
static void *log_calloc(void *ctx, size_t num, size_t size);
static void *log_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size);
static void log_dealloc(void *ctx, void *ptr, size_t size);

/* ========== context ========== */

typedef struct {
    nad_Allocator *wrapped;
    FILE *stream;
} nad_AllocatorLogCtx;

nad_Allocator *nad_allocator_log_new(nad_Allocator *wrapped, FILE *stream) {
    assert(wrapped);
    assert(stream);

    nad_AllocatorLogCtx *ctx = nad_alloc(wrapped, sizeof(nad_AllocatorLogCtx));
    if (!ctx) {
        return nullptr;
    }

    ctx->wrapped = wrapped;
    ctx->stream = stream ? stream : stdout;

    nad_Allocator *alloc = nad_alloc(wrapped, sizeof(nad_Allocator));
    if (!alloc) {
        nad_dealloc(wrapped, ctx, sizeof(nad_AllocatorLogCtx));
        return nullptr;
    }

    alloc->ctx = ctx;
    alloc->alloc = log_alloc;
    alloc->calloc = log_calloc;
    alloc->realloc = log_realloc;
    alloc->dealloc = log_dealloc;

    fprintf(
        ctx->stream,
        "[NAD] log allocator created (wrapping %p)\n",
        (void *) wrapped
    );

    return alloc;
}

void nad_allocator_log_drop(nad_Allocator *self) {
    if (!self) {
        return;
    }

    nad_AllocatorLogCtx *ctx = self->ctx;
    if (!ctx) {
        return;
    }

    fprintf(
        ctx->stream,
        "[NAD] log allocator destroyed (wrapping %p)\n",
        (void *) ctx->wrapped
    );
    fflush(ctx->stream);

    nad_Allocator *wrapped = ctx->wrapped;
    if (wrapped) {
        nad_dealloc(wrapped, self, sizeof(nad_Allocator));
        nad_dealloc(wrapped, ctx, sizeof(nad_AllocatorLogCtx));
    }
}

static void *log_alloc(void *ctx, size_t size) {
    assert(ctx);

    const nad_AllocatorLogCtx *log_ctx = ctx;
    assert(log_ctx->stream);

    void *p = nad_alloc(log_ctx->wrapped, size);
    fprintf(
        log_ctx->stream,
        "[NAD] alloc size = %zu -> %p\n",
        size, p
    );
    fflush(log_ctx->stream);
    return p;
}

static void *log_calloc(void *ctx, size_t num, size_t size) {
    assert(ctx);

    const nad_AllocatorLogCtx *log_ctx = ctx;
    assert(log_ctx->stream);

    void *p = nad_calloc(log_ctx->wrapped, num, size);
    fprintf(
        log_ctx->stream,
        "[NAD] calloc num = %zu size = %zu -> %p\n",
        num, size, p
    );
    fflush(log_ctx->stream);
    return p;
}

static void *log_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    assert(ctx);

    const nad_AllocatorLogCtx *log_ctx = ctx;
    assert(log_ctx->stream);

    void *p = nad_realloc(log_ctx->wrapped, ptr, old_size, new_size);
    fprintf(
        log_ctx->stream,
        "[NAD] realloc %p old size = %zu new_size = %zu -> %p\n",
        ptr, old_size, new_size, p
    );
    fflush(log_ctx->stream);
    return p;
}

static void log_dealloc(void *ctx, void *ptr, size_t size) {
    assert(ctx);

    const nad_AllocatorLogCtx *log_ctx = ctx;
    assert(log_ctx->stream);

    nad_dealloc(log_ctx->wrapped, ptr, size);
    fprintf(
        log_ctx->stream,
        "[NAD] dealloc %p size = %zu\n",
        ptr, size
    );
    fflush(log_ctx->stream);
}
