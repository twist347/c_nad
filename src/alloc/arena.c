#include "terse/alloc/arena.h"

#include "terse/core/util.h"

#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <stddef.h>
#include <string.h>

/* ========== internals ========== */

[[nodiscard]]
static void *arena_alloc(void *ctx, size_t size);

[[nodiscard]]
static void *arena_calloc(void *ctx, size_t num, size_t size);

static void arena_dealloc(void *ctx, void *ptr, size_t size);

typedef struct {
    trs_Al *parent_al;
    void *data;
    size_t cap;
    size_t offset;
} ArenaCtx;

#define ASSERT_ARENA(al)                 \
    (assert(al),                         \
     assert((al)->alloc == arena_alloc), \
     assert((al)->ctx))

/* ========== lifetime ========== */

trs_Al *trs_al_arena_new(trs_Al *parent, size_t cap) {
    assert(parent);
    assert(cap > 0);

    void *data = trs_alloc(parent, cap);
    if (!data) {
        return nullptr;
    }

    ArenaCtx *arena_ctx = trs_alloc(parent, sizeof(ArenaCtx));
    if (!arena_ctx) {
        trs_dealloc(parent, data, cap);
        return nullptr;
    }

    trs_Al *obj = trs_alloc(parent, sizeof(trs_Al));
    if (!obj) {
        trs_dealloc(parent, arena_ctx, sizeof(ArenaCtx));
        trs_dealloc(parent, data, cap);
        return nullptr;
    }

    assert(trs_ptr_is_aligned(data, TRS_DEFAULT_ALIGNMENT));

    arena_ctx->parent_al = parent;
    arena_ctx->data = data;
    arena_ctx->cap = cap;
    arena_ctx->offset = 0;

    obj->ctx = arena_ctx;
    obj->alloc = arena_alloc;
    obj->calloc = arena_calloc;
    obj->realloc = nullptr;
    obj->dealloc = arena_dealloc;

    return obj;
}

void trs_al_arena_drop(trs_Al *self) {
    if (!self) {
        return;
    }

    ASSERT_ARENA(self);

    ArenaCtx *arena_ctx = self->ctx;
    trs_Al *parent_al = arena_ctx->parent_al;
    assert(parent_al);

    trs_dealloc(parent_al, arena_ctx->data, arena_ctx->cap);
    trs_dealloc(parent_al, arena_ctx, sizeof(ArenaCtx));
    trs_dealloc(parent_al, self, sizeof(trs_Al));
}

/* ========== mods ========== */

void trs_al_arena_reset(trs_Al *self) {
    ASSERT_ARENA(self);

    ArenaCtx *arena_ctx = self->ctx;
    arena_ctx->offset = 0;
}

/* ========== stats ========== */

trs_AlArenaStats trs_al_arena_stats(const trs_Al *self) {
    ASSERT_ARENA(self);

    const ArenaCtx *arena_ctx = self->ctx;

    return (trs_AlArenaStats){
        .cap = arena_ctx->cap,
        .used = arena_ctx->offset,
        .available = arena_ctx->cap - arena_ctx->offset,
    };
}

/* ========== internals ========== */

static void *arena_alloc(void *ctx, size_t size) {
    assert(ctx);

    ArenaCtx *arena_ctx = ctx;

    if (size == 0) {
        return nullptr;
    }

    if (size > SIZE_MAX - (TRS_DEFAULT_ALIGNMENT - 1)) {
        return nullptr;
    }
    const size_t aligned_size = trs_align_up(size, TRS_DEFAULT_ALIGNMENT);
    size_t end;
    if (ckd_add(&end, arena_ctx->offset, aligned_size) || end > arena_ctx->cap) {
        return nullptr;
    }

    void *ptr = (unsigned char *) arena_ctx->data + arena_ctx->offset;
    arena_ctx->offset += aligned_size;

    return ptr;
}

static void *arena_calloc(void *ctx, size_t num, size_t size) {
    assert(ctx);

    size_t total;
    if (ckd_mul(&total, num, size)) {
        return nullptr;
    }
    void *ptr = arena_alloc(ctx, total);

    if (ptr) {
        memset(ptr, 0, total);
    }

    return ptr;
}

static void arena_dealloc(void *ctx, void *ptr, size_t size) {
    // arena doesn't free individual allocations

    TRS_UNUSED(ctx);
    TRS_UNUSED(ptr);
    TRS_UNUSED(size);
}
