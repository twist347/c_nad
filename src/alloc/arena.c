#include "nad/alloc/arena.h"

#include "nad/core/util.h"
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
    nad_Al *parent_al;
    void *data;
    size_t cap;
    size_t offset;
} ArenaCtx;

#define ASSERT_ARENA(al)                 \
    (assert(al),                         \
     assert((al)->alloc == arena_alloc), \
     assert((al)->ctx))

/* ========== lifetime ========== */

nad_Al *nad_al_arena_new(nad_Al *parent, size_t cap) {
    assert(parent);
    assert(cap > 0);

    void *data = nad_alloc(parent, cap);
    if (!data) {
        return nullptr;
    }

    ArenaCtx *arena_ctx = nad_alloc(parent, sizeof(ArenaCtx));
    if (!arena_ctx) {
        nad_dealloc(parent, data, cap);
        return nullptr;
    }

    nad_Al *al = nad_alloc(parent, sizeof(nad_Al));
    if (!al) {
        nad_dealloc(parent, arena_ctx, sizeof(ArenaCtx));
        nad_dealloc(parent, data, cap);
        return nullptr;
    }

    assert(nad_ptr_is_aligned(data, NAD_DEFAULT_ALIGNMENT));

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

    ASSERT_ARENA(al);

    ArenaCtx *arena_ctx = al->ctx;
    nad_Al *parent_al = arena_ctx->parent_al;
    assert(parent_al);

    nad_dealloc(parent_al, arena_ctx->data, arena_ctx->cap);
    nad_dealloc(parent_al, arena_ctx, sizeof(ArenaCtx));
    nad_dealloc(parent_al, al, sizeof(nad_Al));
}

/* ========== mods ========== */

void nad_al_arena_reset(nad_Al *al) {
    ASSERT_ARENA(al);

    ArenaCtx *arena_ctx = al->ctx;
    arena_ctx->offset = 0;
}

/* ========== stats ========== */

nad_AlArenaStats nad_al_arena_stats(const nad_Al *al) {
    ASSERT_ARENA(al);

    const ArenaCtx *arena_ctx = al->ctx;

    return (nad_AlArenaStats){
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

    if (size > SIZE_MAX - (NAD_DEFAULT_ALIGNMENT - 1)) {
        return nullptr;
    }
    const size_t aligned_size = nad_align_up(size, NAD_DEFAULT_ALIGNMENT);
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

    NAD_UNUSED(ctx);
    NAD_UNUSED(ptr);
    NAD_UNUSED(size);
}
