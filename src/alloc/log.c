#include "nad/alloc/log.h"

#include "nad/alloc/alloc.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

/* ========== internals ========== */

[[nodiscard]]
static void *log_alloc(void *ctx, size_t size);

[[nodiscard]]
static void *log_calloc(void *ctx, size_t num, size_t size);

[[nodiscard]]
static void *log_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size);

static void log_dealloc(void *ctx, void *ptr, size_t size);

typedef struct {
    nad_Al *wrapped;
    FILE *stream;
} LogCtx;

#define ASSERT_LOG(al)                 \
    (assert(al),                       \
     assert((al)->alloc == log_alloc), \
     assert((al)->ctx))

/* ========== lifetime ========== */

nad_Al *nad_al_log_new(nad_Al *wrapped, FILE *stream) {
    assert(wrapped);
    assert(stream);

    LogCtx *log_ctx = nad_alloc(wrapped, sizeof(LogCtx));
    if (!log_ctx) {
        return nullptr;
    }

    log_ctx->wrapped = wrapped;
    log_ctx->stream = stream;

    nad_Al *alloc = nad_alloc(wrapped, sizeof(nad_Al));
    if (!alloc) {
        nad_dealloc(wrapped, log_ctx, sizeof(LogCtx));
        return nullptr;
    }

    alloc->ctx = log_ctx;
    alloc->alloc = log_alloc;
    alloc->calloc = log_calloc;
    alloc->realloc = log_realloc;
    alloc->dealloc = log_dealloc;

    fprintf(
        log_ctx->stream,
        "[NAD] log allocator created (wrapping %p)\n",
        (void *) wrapped
    );

    return alloc;
}

void nad_al_log_drop(nad_Al *self) {
    if (!self) {
        return;
    }

    ASSERT_LOG(self);

    LogCtx *log_ctx = self->ctx;

    fprintf(
        log_ctx->stream,
        "[NAD] log allocator destroyed (wrapping %p)\n",
        (void *) log_ctx->wrapped
    );
    fflush(log_ctx->stream);

    nad_Al *wrapped = log_ctx->wrapped;
    assert(wrapped);

    nad_dealloc(wrapped, self, sizeof(nad_Al));
    nad_dealloc(wrapped, log_ctx, sizeof(LogCtx));
}

/* ========== internals ========== */

static void *log_alloc(void *ctx, size_t size) {
    assert(ctx);

    const LogCtx *log_ctx = ctx;
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

    const LogCtx *log_ctx = ctx;
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

    const LogCtx *log_ctx = ctx;
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

    const LogCtx *log_ctx = ctx;
    assert(log_ctx->stream);

    fprintf(
        log_ctx->stream,
        "[NAD] dealloc %p size = %zu\n",
        ptr, size
    );
    fflush(log_ctx->stream);
    nad_dealloc(log_ctx->wrapped, ptr, size);
}
