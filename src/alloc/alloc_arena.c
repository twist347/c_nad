#include "nad/alloc/alloc_arena.h"

#include "nad/core/util.h"
#include "internal/ptr.h"

#include <assert.h>
#include <stdckdint.h>
#include <stddef.h>
#include <string.h>

/* ========== internals ========== */

static constexpr size_t DEFAULT_ALIGNMENT = alignof(max_align_t);

[[nodiscard]] static
void *arena_alloc(void *ctx, size_t size);

[[nodiscard]] static
void *arena_calloc(void *ctx, size_t num, size_t size);

static
void arena_dealloc(void *ctx, void *ptr, size_t size);

typedef struct {
    nad_Al *parent_al;
    void *data;
    size_t cap;
    size_t offset;
} nad_AlArenaCtx;

/* ========== lifetime ========== */

nad_Al *nad_al_arena_new(nad_Al *parent, size_t cap) {
    assert(parent);
    assert(cap > 0);

    void *data = nad_alloc(parent, cap);
    if (!data) {
        return nullptr;
    }

    nad_AlArenaCtx *arena_ctx = nad_alloc(parent, sizeof(nad_AlArenaCtx));
    if (!arena_ctx) {
        nad_dealloc(parent, data, cap);
        return nullptr;
    }

    nad_Al *al = nad_alloc(parent, sizeof(nad_Al));
    if (!al) {
        nad_dealloc(parent, arena_ctx, sizeof(nad_AlArenaCtx));
        nad_dealloc(parent, data, cap);
        return nullptr;
    }

    assert(nad_ptr_is_aligned(data, DEFAULT_ALIGNMENT));

    arena_ctx->parent_al = parent;
    arena_ctx->data = data;
    arena_ctx->cap = cap;
    arena_ctx->offset = 0;

    al->ctx = arena_ctx;
    al->alloc = arena_alloc;
    al->calloc = arena_calloc;
    al->realloc = nullptr;
    al->dealloc = arena_dealloc;

    return al;
}

void nad_al_arena_drop(nad_Al *al) {
    if (!al) {
        return;
    }

    assert(al->ctx);

    nad_AlArenaCtx *arena_ctx = al->ctx;
    nad_Al *parent_al = arena_ctx->parent_al;
    assert(parent_al);

    nad_dealloc(parent_al, arena_ctx->data, arena_ctx->cap);
    nad_dealloc(parent_al, arena_ctx, sizeof(nad_AlArenaCtx));
    nad_dealloc(parent_al, al, sizeof(nad_Al));
}

/* ========== mods ========== */

void nad_al_arena_reset(nad_Al *al) {
    assert(al);
    assert(al->ctx);

    nad_AlArenaCtx *arena_ctx = al->ctx;
    arena_ctx->offset = 0;
}

/* ========== stats ========== */

nad_AlArenaStats nad_al_arena_stats(const nad_Al *al) {
    assert(al);
    assert(al->ctx);

    const nad_AlArenaCtx *arena_ctx = al->ctx;

    return (nad_AlArenaStats){
        .cap = arena_ctx->cap,
        .used = arena_ctx->offset,
        .available = arena_ctx->cap - arena_ctx->offset,
    };
}

/* ========== internals ========== */

static
void *arena_alloc(void *ctx, size_t size) {
    assert(ctx);

    nad_AlArenaCtx *arena_ctx = ctx;

    if (size == 0) {
        return nullptr;
    }

    if (size > SIZE_MAX - (DEFAULT_ALIGNMENT - 1)) {
        return nullptr;
    }
    const size_t aligned_size = nad_align_up(size, DEFAULT_ALIGNMENT);
    size_t end;
    if (ckd_add(&end, arena_ctx->offset, aligned_size) || end > arena_ctx->cap) {
        return nullptr;
    }

    void *ptr = (char *) arena_ctx->data + arena_ctx->offset;
    arena_ctx->offset += aligned_size;

    return ptr;
}

static
void *arena_calloc(void *ctx, size_t num, size_t size) {
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

static
void arena_dealloc(void *ctx, void *ptr, size_t size) {
    // arena doesn't free individual allocations

    NAD_UNUSED(ctx);
    NAD_UNUSED(ptr);
    NAD_UNUSED(size);
}
