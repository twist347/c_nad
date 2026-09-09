#include "terse/alloc/log.h"

#include "terse/alloc/alloc.h"

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
    trs_Al *wrapped;
    FILE *stream;
} LogCtx;

#define ASSERT_LOG(al)                 \
    (assert(al),                       \
     assert((al)->alloc == log_alloc), \
     assert((al)->ctx))

/* ========== lifetime ========== */

trs_Al *trs_al_log_new(trs_Al *wrapped, FILE *stream) {
    assert(wrapped);
    assert(stream);

    LogCtx *log_ctx = trs_alloc(wrapped, sizeof(LogCtx));
    if (!log_ctx) {
        return nullptr;
    }

    log_ctx->wrapped = wrapped;
    log_ctx->stream = stream;

    trs_Al *obj = trs_alloc(wrapped, sizeof(trs_Al));
    if (!obj) {
        trs_dealloc(wrapped, log_ctx, sizeof(LogCtx));
        return nullptr;
    }

    obj->ctx = log_ctx;
    obj->alloc = log_alloc;
    obj->calloc = log_calloc;
    obj->realloc = log_realloc;
    obj->dealloc = log_dealloc;

    fprintf(
        log_ctx->stream,
        "[TRS] log allocator created (wrapping %p)\n",
        (void *) wrapped
    );

    return obj;
}

void trs_al_log_drop(trs_Al *self) {
    if (!self) {
        return;
    }

    ASSERT_LOG(self);

    LogCtx *log_ctx = self->ctx;

    fprintf(
        log_ctx->stream,
        "[TRS] log allocator destroyed (wrapping %p)\n",
        (void *) log_ctx->wrapped
    );
    fflush(log_ctx->stream);

    trs_Al *wrapped = log_ctx->wrapped;
    assert(wrapped);

    trs_dealloc(wrapped, log_ctx, sizeof(LogCtx));
    trs_dealloc(wrapped, self, sizeof(trs_Al));
}

/* ========== internals ========== */

static void *log_alloc(void *ctx, size_t size) {
    assert(ctx);

    const LogCtx *log_ctx = ctx;
    assert(log_ctx->stream);

    void *p = trs_alloc(log_ctx->wrapped, size);
    fprintf(
        log_ctx->stream,
        "[TRS] alloc size = %zu -> %p\n",
        size, p
    );
    fflush(log_ctx->stream);
    return p;
}

static void *log_calloc(void *ctx, size_t num, size_t size) {
    assert(ctx);

    const LogCtx *log_ctx = ctx;
    assert(log_ctx->stream);

    void *p = trs_calloc(log_ctx->wrapped, num, size);
    fprintf(
        log_ctx->stream,
        "[TRS] calloc num = %zu size = %zu -> %p\n",
        num, size, p
    );
    fflush(log_ctx->stream);
    return p;
}

static void *log_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    assert(ctx);

    const LogCtx *log_ctx = ctx;
    assert(log_ctx->stream);

    void *p = trs_realloc(log_ctx->wrapped, ptr, old_size, new_size);
    fprintf(
        log_ctx->stream,
        "[TRS] realloc %p old size = %zu new_size = %zu -> %p\n",
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
        "[TRS] dealloc %p size = %zu\n",
        ptr, size
    );
    fflush(log_ctx->stream);
    trs_dealloc(log_ctx->wrapped, ptr, size);
}
